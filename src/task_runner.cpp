#include "task_runner.h"
#include "clock.h"
#include <thread>

namespace posix_quic {

std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask>
QuicTaskRunner::Schedule(Task* task, uint64_t delay_ms)
{
    int64_t d = QuicClockImpl::getInstance().NowMicroseconds() / 1000 + delay_ms;
    TaskStoragePtr storage(new TaskStorage);
    storage->task = task;
    storage->runner_ = this;

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
    std::thread t([this]{ this->ThreadRun(); });
    t.detach();
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
            if (!storage->invalid.test_and_set(std::memory_order_acquire)) {
                triggers.push_back(storage);
                itr = tasks_.erase(itr);
            } else {
                ++itr;
            }
        }
    }

    for (auto & storage : triggers) {
        DebugPrint(dbg_timer, "start trigger schedule(id=%ld) task-count=%d", storage->id, (int)tasks_.size());

        {
            std::unique_lock<SpinLock> lock(storage->callLock, std::defer_lock);
            if (lock.try_lock())
                storage->task->Run();
        }

        DebugPrint(dbg_timer, "end trigger schedule(id=%ld) task-count=%d", storage->id, (int)tasks_.size());
    }

    ScheduledTask::IsInTaskRunnerThread() = false;
}

void QuicTaskRunner::ThreadRun()
{
    for (;;) {
        usleep(10 * 1000);

        RunOnce();
    }
}

std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask>
QuicTaskRunnerProxy::Schedule(Task* task, uint64_t delay_ms)
{
    int64_t deadline = QuicClockImpl::getInstance().NowMicroseconds() / 1000 + delay_ms;
    StoragePtr storage(new Storage(task, deadline, this));
    storages_[storage.get()] = storage;
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
        storage->link_ = runner_->Schedule(storage->task_, delay_ms);
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

