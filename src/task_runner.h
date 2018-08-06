#pragma once

#include "fwd.h"
#include <mutex>
#include <map>
#include <atomic>

namespace posix_quic {

// 定时器
class QuicTaskRunner
    : public QuartcTaskRunnerInterface
{
public:
    static QuicTaskRunner& getInstance();

    using QuartcTaskRunnerInterface::Task;

    struct TaskStorage;
    typedef std::shared_ptr<TaskStorage> TaskStoragePtr;

    typedef std::multimap<int64_t, TaskStoragePtr> TaskMap;

    struct TaskStorage {
        Task * task;
        TaskMap::iterator itr;
        std::atomic_flag invalid{false};
        long id;

        inline static long& StaticTaskId() {
            static long taskId = 0;
            return taskId;
        }
    };

    struct ScheduledTask
        : public QuartcTaskRunnerInterface::ScheduledTask
    {
    public:
        explicit ScheduledTask(TaskStoragePtr storage) : storage_(storage) {}

        void Cancel() override {
            if (!storage_->invalid.test_and_set(std::memory_order_acquire)) {
                DebugPrint(dbg_timer, "cancel schedule(id=%ld)", storage_->id);
                QuicTaskRunner::getInstance().Cancel(storage_->itr);
            }
        }

    private:
        TaskStoragePtr storage_;
    };

    std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask>
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
