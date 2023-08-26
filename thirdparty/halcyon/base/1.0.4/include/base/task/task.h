#ifndef BASE_TASK_H
#define BASE_TASK_H

#include <base/common/base_define.h>
#ifdef USE_HALCYON_ANY
#include <base/any/any.h>
#else
#include <any>
#endif
#include <mutex>
#include <memory>
#include <functional>

BASE_BEGIN_NAMESPACE

class Task;
inline void defaultCallback(Task*)
{}

/**
 * @brief   任务基类
 * @ps      可以异步获取任务结果，又需要擦除结果类型，方便将任务
 *        塞进队列，创建了基类... 参考py
 *        是否有其他更好的方法？任务结果在子类中。
 */
class Task
{
public:
    /// 任务状态
    enum Status
    {
        emPending,
        emRunning,
        emCancelled,
        emFinished
    };
    /// 任务运行完成后的回调
    using callback_t = std::function<void(Task*)>;

public:
    Task() = default;
    virtual ~Task() = default;

public:
    /**
     * @brief   获取任务状态
     */
    Status status() const
    {
        std::lock_guard<std::mutex> locker(mutex_);
        return status_;
    }

    /**
     * @brief   取消任务
     * @return  是否成功
     */
    bool cancel()
    {
        {
            std::lock_guard<std::mutex> locker(mutex_);
            if (status_ == emRunning || status_ == emFinished)
                return false;

            if (status_ == emCancelled)
                return true;

            status_ = emCancelled;
        }
        callback_(this);
        return true;
    }

    /**
     * @brief   任务是否取消
     * @return  是否取消
     */
    bool cancelled() const
    {
        std::lock_guard<std::mutex> locker(mutex_);
        return status_ == emCancelled;
    }

    /**
     * @brief   任务是否正在运行
     */
    bool running() const
    {
        std::lock_guard<std::mutex> locker(mutex_);
        return status_ == emRunning;
    }

    /**
     * @brief   任务是否完成
     */
    bool done() const
    {
        std::lock_guard<std::mutex> locker(mutex_);
        return status_ == emCancelled || status_ == emFinished;
    }

    /**
     * @brief   设置任务完成后的回调函数
     * @ps      若任务已完成，则立即调用回调函数；
     *        是否需要支持多个 callback?
     */
    void setDoneCallback(callback_t&& call)
    {
        if (call == nullptr)
            return;
        {
            std::lock_guard<std::mutex> locker(mutex_);
            if ((status_ != emCancelled) && (status_ != emFinished)) {
                callback_ = call;
                return;
            }
        }
        call(this);
    }

    /**
     * @brief       等待任务结束
     * @param[in]   超时时间(ms)
     * @return      是否成功，timeout 为 0，会阻塞直到任务执行完成；
     *            非0，则 timeout 时间内没有结果返回失败。
     */
    virtual bool wait(uint64_t timeout = 0) = 0;

    /**
     * @brief       获取任务结果
     * @param[out]  任务结果
     * @param[in]   超时时间(ms)
     * @return      是否成功，timeout 为 0，会阻塞直到任务执行完成；
     *            非0，则 timeout 时间内没有结果返回失败。
     * @ps          是否可以不用 any 实现？
     */
#ifdef USE_HALCYON_ANY
    virtual bool result(Any& value, uint64_t timeout = 0) = 0;
#else
    virtual bool result(std::any& value, uint64_t timeout = 0) = 0;
#endif

protected:
    /**
     * @brief   设置运行状态，如果任务被取消则失败。
     */
    bool setRunning()
    {
        std::lock_guard<std::mutex> locker(mutex_);
        if (status_ == emCancelled)
            return false;
        status_ = emRunning;
        return true;
    }

    /**
     * @brief   设置运行完成状态
     */
    void setFinished()
    {
        std::lock_guard<std::mutex> locker(mutex_);
        status_ = emFinished;
    }

private:
    friend class Thread;  // 线程
    friend class TimerLoop;  // 定时器
    friend class ThreadPoolImpl;  // 线程池

private:
    /**
     * @brief   运行任务
     */
    virtual void run() = 0;

protected:
    //! 任务执行完成后的回调函数
    callback_t callback_{ &defaultCallback };

    //! 任务状态
    Status status_{ emPending };
    mutable std::mutex mutex_;
};

using TaskSPtr = std::shared_ptr<Task>;
using TaskWPtr = std::weak_ptr<Task>;

BASE_END_NAMESPACE

#endif