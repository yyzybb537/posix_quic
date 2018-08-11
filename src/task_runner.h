#pragma once

#include "fwd.h"
#include "spinlock.h"
#include <mutex>
#include <map>
#include <atomic>

namespace posix_quic {

// 定时器
class QuicTaskRunner
    : public QuartcTaskRunnerInterface
{
public:
    using QuartcTaskRunnerInterface::Task;

    struct TaskStorage;
    typedef std::shared_ptr<TaskStorage> TaskStoragePtr;

    typedef std::multimap<int64_t, TaskStoragePtr> TaskMap;

    struct TaskStorage {
        long id;
        Task * task;
        TaskMap::iterator itr;
        std::atomic_flag invalid{false};
        SpinLock callLock;
        QuicTaskRunner * runner_;

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
                storage_->runner_->Cancel(storage_->itr);
            } else if (!IsInTaskRunnerThread()) {
                storage_->callLock.lock();
            }
        }
        
    private:
        friend class QuicTaskRunner;

        static bool & IsInTaskRunnerThread() {
            static thread_local bool b = false;
            return b;
        }

    private:
        TaskStoragePtr storage_;
    };

    std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask>
        Schedule(Task* task, uint64_t delay_ms) override;

    void Cancel(TaskMap::iterator itr);

    void RunOnce();

    QuicTaskRunner();

private:
    void ThreadRun();

private:
    std::mutex mtx_;
    TaskMap tasks_;
};

class QuicTaskRunnerProxy
    : public QuartcTaskRunnerInterface
{
public:
    struct Storage {
        Storage(Task* task, uint64_t deadline, QuicTaskRunnerProxy * proxy)
            : task_(task), deadline_(deadline), proxy_(proxy), id_(++StaticTaskId())
        {}

        inline static long& StaticTaskId() {
            static long taskId = 0;
            return taskId;
        }

        long id_;
        Task * task_;
        uint64_t deadline_;
        QuicTaskRunnerProxy * proxy_;
        std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask> link_;
    };
    typedef std::shared_ptr<Storage> StoragePtr;

    struct ProxyScheduledTask
        : public QuartcTaskRunnerInterface::ScheduledTask
    {
    public:
        ProxyScheduledTask(StoragePtr storage)
            : storage_(storage) {}

        void Cancel() override {
            if (storage_) {
                storage_->proxy_->Cancel(storage_);
                storage_.reset();
            }
        }

    private:
        StoragePtr storage_;
    };

    std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask>
        Schedule(Task* task, uint64_t delay_ms) override;

    bool Link(QuicTaskRunner *runner);

    void Unlink();

    void Cancel(StoragePtr storage);

private:
    std::unordered_map<Storage*, StoragePtr> storages_;

    QuicTaskRunner *runner_ = nullptr;
};

} // namespace posix_quic
