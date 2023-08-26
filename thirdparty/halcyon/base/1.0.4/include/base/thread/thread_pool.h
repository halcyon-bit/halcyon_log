#ifndef BASE_THREAD_POOL_H
#define BASE_THREAD_POOL_H

#include <base/common/noncopyable.h>
#include <base/thread/thread_task.h>

#include <thread>

BASE_BEGIN_NAMESPACE

class ThreadPoolImpl;

/// 线程池(调整任务类型为 ThreadTask)
class HALCYON_BASE_API ThreadPool final : noncopyable
{
public:
    /**
     * @brief   构造函数
     */
    explicit ThreadPool() noexcept;

    /**
     * @brief   析构函数
     */
    ~ThreadPool();

    /**
     * @brief       启动线程池
     * @param[in]   线程数量
     * @return      是否启动成功
     */
    bool start(int32_t num_threads = std::thread::hardware_concurrency());

    /**
     * @brief   获取正在处于等待状态的线程个数
     */
    size_t getWaitingThreadNum() const;

    /**
     * @brief   获取线程池中当前线程的总个数
     */
    size_t getTotalThreadNum() const;

    /**
     * @brief       添加任务
     * @param[in]   任务函数
     * @param[in]   任务参数
     * @return      任务对象，失败返回 nullptr(例如线程池已经停止)
     */
    template <typename F, typename... Args>
    TaskSPtr push(F&& func, Args&&... args)
    {
#if defined USE_CPP11 || defined USE_CPP14
        using return_type = std::result_of_t<F(Args...)>;
#else
        using return_type = std::invoke_result_t<F, Args...>;
#endif
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(func), std::forward<Args>(args)...));

        TaskSPtr result = std::make_shared<ThreadTask<return_type>>(
            [task]() { (*task)(); },
            task->get_future());

        return addTask(result);
    }

    // 针对 nullptr 
    void push(std::nullptr_t) = delete;

    /**
     * @brief   停止线程池，还没有执行的任务会继续执行
     *        直到任务全部执行完成
     */
    void shutDown();

    /**
     * @brief   停止线程池，还没有执行的任务直接取消，不会再执行
     */
    void shutDownNow();

private:
    /**
     * @brief   添加任务至队列中
     */
    TaskSPtr addTask(TaskSPtr task);

private:
    ThreadPoolImpl* impl_;
};

BASE_END_NAMESPACE

#endif