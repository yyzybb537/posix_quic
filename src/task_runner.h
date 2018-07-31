#pragma once

#include "net/quic/quartc/quartc_task_runner_interface.h"
#include <mutex>
#include <map>
#include <atomic>

namespace posix_quic {

// 定时器
class QuicTaskRunner
    : public net::QuartcTaskRunnerInterface
{
public:
    static QuicTaskRunner& getInstance();

    struct TaskStorage {
        Task * task;
        TaskMap::iterator itr;
        std::atomic_flag invalid{false};
    };
    typedef std::shared_ptr<TaskStorage> TaskStoragePtr;

    typedef std::multimap<int64_t, TaskStoragePtr> TaskMap;

    struct ScheduledTask
        : public net::QuartcTaskRunnerInterface::ScheduledTask
    {
    public:
        explicit ScheduledTask(TaskStoragePtr storage) : storage_(storage) {}

        void Cancel() override {
            if (!storage_->invalid.test_and_set(std::memory_order_acquire)) {
                QuicTaskRunner::getInstance().Cancel(storage_->itr);
            }
        }

    private:
        TaskStoragePtr storage_;
    };

    std::unique_ptr<net::QuartcTaskRunnerInterface::ScheduledTask>
        Schedule(Task* task, uint64_t delay_ms) override;

    void Cancel(TaskMap::iterator itr);

private:
    QuicTaskRunner();

    void ThreadRun();

private:
    std::mutex mtx_;
    TaskMap tasks_;
};

} // namespace posix_quic
