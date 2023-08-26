#ifndef BASE_COUNT_DOWN_LATCH_H
#define BASE_COUNT_DOWN_LATCH_H

#include <base/common/noncopyable.h>

#include <mutex>
#include <chrono>
#include <condition_variable>

BASE_BEGIN_NAMESPACE

/**
 * @brief   同步工具类
 *      能够使一个或多个线程在等待另外一些线程完成各自工作之后，再继续执行。
 *  使用一个计数器进行实现。计数器初始值为线程的数量。当每一个线程完成自己任
 *  务后，计数器的值就会减一。当计数器的值为0时，表示所有的线程都已经完成
 *  任务，然后在 CountDownLatch 上等待的线程就可以恢复执行接下来的任务。
 */
class CountDownLatch final : noncopyable
{
public:
    /**
     * @brief       构造函数
     * @param[in]   计数器值
     */
    explicit CountDownLatch(int32_t count)
        : count_(count)
    {}

    /**
     * @brief   等待其他线程执行完成
     */
    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (count_ > 0) {
            cv_.wait(lock);
        }
    }

    /**
     * @brief       超时等待其他线程执行完成
     * @param[in]   超时时长(ms)
     * return       是否超时, 未超时: true，超时: false
     */
    bool waitFor(uint32_t timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout), [&] { return !(count_ > 0); });
    }

    /**
     * @brief   使计数器减一，当计数器为0时，唤醒所有线程
     */
    void countDown()
    {
        int32_t count;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --count_;
            count = count_;
        }
        if (count == 0) {
            cv_.notify_all();
        }
    }

    /**
     * @brief   获取计数器的值
     */
    int32_t getCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    //! 计数器
    int32_t count_;
};

BASE_END_NAMESPACE

#endif