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

QuicTaskRunner & QuicTaskRunner::getInstance()
{
    static QuicTaskRunner obj;
    return obj;
}

QuicTaskRunner::QuicTaskRunner()
{
    std::thread t([this]{ this->ThreadRun(); });
    t.detach();
}

void QuicTaskRunner::ThreadRun()
{
    for (;;) {
        usleep(10 * 1000);

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
            storage->task->Run();
            DebugPrint(dbg_timer, "end trigger schedule(id=%ld) task-count=%d", storage->id, (int)tasks_.size());
        }
    }
}

} // namespace posix_quic

