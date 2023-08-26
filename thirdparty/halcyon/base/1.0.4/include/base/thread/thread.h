#ifndef BASE_THREAD_H
#define BASE_THREAD_H

#include <base/thread/thread_task.h>
#include <base/queue/blocking_queue.h>

#include <thread>
#include <atomic>

BASE_BEGIN_NAMESPACE

/**
 * @brief   线程封装
 * @ps      最初版本中线程任务为 std::function<void()>，但获取不到任务执行
 *        结果，后通过继承调整了任务类型(ThreadTask)，是否有更优的方法解决？
 *        性能略微有所下降
 */
class Thread final : noncopyable
{
public:
    // using Task = std::function<void()>;  //! 任务函数

public:
    /**
     * @brief   构造函数
     */
    Thread()
    {
        started_ = true;
        thd_ = std::thread(&Thread::threadProc, this);
    }

    /**
     * @brief   析构函数
     */
    ~Thread()
    {
        if (thd_.joinable()) {
            join();
        }
    }

    /**
     * @brief       添加任务
     * @param[in]   函数
     * @param[in]   函数参数
     * @return      任务对象，失败返回 nullptr(例如线程已经停止)
     */
    template<typename F, typename... Args>
    TaskSPtr push(F&& func, Args&&... args)
    {
        if (!started_.load(std::memory_order_acquire)) {
            return nullptr;
        }

        // 返回类型
#if defined USE_CPP11 || defined USE_CPP14
        using return_type = std::result_of_t<F(Args...)>;
#else
        using return_type = std::invoke_result_t<F, Args...>;
#endif
        // 任务
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(func), std::forward<Args>(args)...));

        TaskSPtr result = std::make_shared<ThreadTask<return_type>>(
            [task]() { (*task)(); },
            task->get_future());

        queue_.push(result);
        return result;
    }

    // 针对 nullptr 
    void push(std::nullptr_t) = delete;

    /**
     * @brief   判断线程是否可以 join
     */
    bool joinable()
    {
        return thd_.joinable();
    }

    /**
     * @brief   是否启动
     */
    bool started()
    {
        return started_.load(std::memory_order_acquire);
    }

    /**
     * @brief   设置退出信号，并等待线程执行完成
     */
    void join()
    {
        bool expected = true;
        if (started_.compare_exchange_strong(expected, false)) {
            queue_.push(nullptr);
            thd_.join();
        }
    }

private:
    /**
     * @brief   线程函数
     */
    void threadProc()
    {
        while (true) {
            auto task = queue_.take();
            if (task == nullptr) {
                // 线程结束标志
                break;
            }
            task->run();
        }
    }

private:
    //! 线程
    std::thread thd_;
    //! 任务队列
    BlockingQueue<TaskSPtr> queue_;
    //! 是否启动
    std::atomic_bool started_;
};

using ThreadSPtr = std::shared_ptr<Thread>;
using ThreadWPtr = std::weak_ptr<Thread>;

BASE_END_NAMESPACE

#endif