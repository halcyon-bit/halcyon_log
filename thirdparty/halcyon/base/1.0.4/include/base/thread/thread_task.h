#ifndef BASE_THREAD_TASK_H
#define BASE_THREAD_TASK_H

#include <base/task/task.h>

#include <future>

BASE_BEGIN_NAMESPACE

/// 线程任务
template<typename T>
class ThreadTask final : public Task
{
public:
    ThreadTask(std::function<void()>&& func, std::future<T>&& result)
        : func_(std::move(func))
        , result_(std::move(result))
    {}
    ~ThreadTask() = default;

public:
    /**
     * @brief       等待任务结束
     * @param[in]   超时时间(ms)
     * @return      是否成功，timeout 为 0，会阻塞直到任务执行完成；
     *            非0，则 timeout 时间内没有结果返回失败。
     */
    bool wait(uint64_t timeout = 0) override
    {
        if (timeout == 0) {
            result_.wait();
            return true;
        } else {
            std::chrono::milliseconds span(timeout);
            return result_.wait_for(span) == std::future_status::timeout;
        }
    }

    /**
     * @brief       获取任务结果
     * @param[out]  任务结果
     * @param[in]   超时时间(ms)
     * @return      是否成功，timeout 为 0，会阻塞直到任务执行完成；
     *            非0，则 timeout 时间内没有结果返回失败。
     * @ps          是否可以不用 any 实现？
     */
#ifdef USE_HALCYON_ANY
    bool result(Any& value, uint64_t timeout = 0) override
#else
    bool result(std::any& value, uint64_t timeout = 0) override
#endif
    {
        if (timeout == 0) {
            resultAux(value, std::is_same<T, void>());
            return true;
        }
        std::chrono::milliseconds span(timeout);
        if (result_.wait_for(span) == std::future_status::timeout) {
            return false;
        } else {
            resultAux(value, std::is_same<T, void>());
            return true;
        }
    }

private:
    /**
     * @brief   处理返回值为 void 类型的情况
     */
#ifdef USE_HALCYON_ANY
    void resultAux(Any& value, std::true_type)
#else
    void resultAux(std::any& value, std::true_type)
#endif
    {
        result_.wait();
    }

#ifdef USE_HALCYON_ANY
    void resultAux(Any& value, std::false_type)
#else
    void resultAux(std::any& value, std::false_type)
#endif
    {
        value = result_.get();
    }

private:
    /**
     * @brief   运行任务
     */
    void run() override
    {
        if (!setRunning())  // 设置运行态
            return;
        func_();
        setFinished();  // 设置完成态
        callback_(this);  // 通知结果
    }

private:
    //! 任务函数
    std::function<void()> func_;
    //! 任务结果
    std::future<T> result_;
};

BASE_END_NAMESPACE

#endif