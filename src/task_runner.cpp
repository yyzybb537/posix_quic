#include "task_runner.h"
#include "clock.h"
#include <thread>

namespace posix_quic {

std::unique_ptr<net::QuartcTaskRunnerInterface::ScheduledTask>
QuicTaskRunner::Schedule(Task* task, uint64_t delay_ms)
{
    int64_t d = QuicClockImpl::getInstance().NowMicroseconds() / 1000 + delay_ms;
    TaskStoragePtr storage(new TaskStorage);
    storage->task = task;

    std::unique_lock<std::mutex> lock(mtx_);
    auto itr = tasks_.insert(TaskMap::value_type(d, storage));
    storage->itr = itr;
    lock.unlock();

    return static_cast<net::QuartcTaskRunnerInterface::ScheduledTask*>(new ScheduledTask(storage));
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

        std::vector<TaskStoragePtr> triggers;

        {
            std::unique_lock<std::mutex> lock(mtx_);
            auto itr = tasks_.begin();
            while (itr != tasks_.end() && itr->first > now) {
                TaskStoragePtr & storage = itr->second;
                if (!storage.invalid.test_and_set(std::memory_order_acquire)) {
                    triggers.push_back(storage);
                    itr = tasks_.erase(itr);
                } else {
                    ++itr;
                }
            }
        }

        for (auto & storage : triggers) {
            storage->task->Run();
        }
    }
}

} // namespace posix_quic

