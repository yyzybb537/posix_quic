#include "task_runner.h"
#include "clock.h"
#include <thread>

namespace posix_quic {

std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask>
QuicTaskRunner::Schedule(Task* task, uint64_t delay_ms, uint64_t connectionId,
        std::shared_ptr<std::recursive_mutex> mtx)
{
    int64_t d = QuicClockImpl::getInstance().NowMicroseconds() / 1000 + delay_ms;
    TaskStoragePtr storage(new TaskStorage);
    storage->task = task;
    storage->runner_ = this;
    storage->connectionId = connectionId;
    storage->mtx = mtx;

    std::unique_lock<std::mutex> lock(mtx_);
    auto itr = tasks_.insert(TaskMap::value_type(d, storage));
    storage->itr = itr;
    storage->id = ++TaskStorage::StaticTaskId();
    lock.unlock();

    DebugPrint(dbg_timer, "add schedule(id=%ld, delay_ms=%d) dest-time=%ld", storage->id, (int)delay_ms, d);

    return std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask>(
            static_cast<QuartcTaskRunnerInterface::ScheduledTask*>(new ScheduledTask(storage)));
}

void QuicTaskRunner::Cancel(TaskMap::iterator itr)
{
    std::unique_lock<std::mutex> lock(mtx_);
    tasks_.erase(itr);
}

QuicTaskRunner::QuicTaskRunner()
{
}

void QuicTaskRunner::RunOnce()
{
    ScheduledTask::IsInTaskRunnerThread() = true;
    int64_t now = QuicClockImpl::getInstance().NowMicroseconds() / 1000;

    //        DebugPrint(dbg_timer, "Timer loop: now=%ld, first=%ld",
    //                now, tasks_.empty() ? -1 : tasks_.begin()->first );

    std::vector<TaskStoragePtr> triggers;

    {
        std::unique_lock<std::mutex> lock(mtx_);
        auto itr = tasks_.begin();
        while (itr != tasks_.end() && now > itr->first) {
            TaskStoragePtr & storage = itr->second;
            if (storage->invalid.try_lock()) {
                triggers.push_back(storage);
                itr = tasks_.erase(itr);
            } else {
                ++itr;
            }
        }
    }

    for (auto & storage : triggers) {
        DebugPrint(dbg_timer, "start trigger schedule(id=%ld) task-count=%d", storage->id, (int)tasks_.size());

        // 其他线程Writev的时候会先lock住std::recursive_mutex, 然后再调用Cancel
        // 此处直接lock等待会导致ABBA死锁, 因此这里做一个活锁.
lock_retry:

        {
            std::unique_lock<SpinLock> lock(storage->callLock, std::defer_lock);
            if (lock.try_lock()) {
                TlsConnectionIdGuard guard(storage->connectionId);
                std::unique_lock<std::recursive_mutex> rlock(*storage->mtx, std::defer_lock);
                if (!rlock.try_lock()) {
                    goto lock_retry;
                }

                storage->task->Run();
            }
        }

        DebugPrint(dbg_timer, "end trigger schedule(id=%ld) task-count=%d", storage->id, (int)tasks_.size());
    }

    ScheduledTask::IsInTaskRunnerThread() = false;
}

std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask>
QuicTaskRunnerProxy::Schedule(Task* task, uint64_t delay_ms)
{
    int64_t deadline = QuicClockImpl::getInstance().NowMicroseconds() / 1000 + delay_ms;
    StoragePtr storage(new Storage(task, deadline, this));
    storages_[storage.get()] = storage;
    if (runner_) {
        storage->link_ = runner_->Schedule(storage->task_, delay_ms, connectionId_, mtx_);
    }
    return std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask>(
            static_cast<QuartcTaskRunnerInterface::ScheduledTask*>(
                new ProxyScheduledTask(storage)));
}

bool QuicTaskRunnerProxy::Link(QuicTaskRunner *runner)
{
    if (runner_) return false;
    runner_ = runner;
    for (auto & kv : storages_) {
        StoragePtr & storage = kv.second;
        int64_t now = QuicClockImpl::getInstance().NowMicroseconds() / 1000;
        uint64_t delay_ms = now > storage->deadline_ ? 0 : (storage->deadline_ - now);
        storage->link_ = runner_->Schedule(storage->task_, delay_ms, connectionId_, mtx_);
    }
    return true;
}

void QuicTaskRunnerProxy::Unlink()
{
    if (runner_) {
        for (auto & kv : storages_) {
            StoragePtr & storage = kv.second;
            if (storage->link_) {
                storage->link_->Cancel();
                storage->link_.reset();
            }
        }
    }
    runner_ = nullptr;
}

void QuicTaskRunnerProxy::Cancel(StoragePtr storage)
{
    if (storage->link_) {
        storage->link_->Cancel();
        storage->link_.reset();
    }
    storages_.erase(storage.get());
}

} // namespace posix_quic

