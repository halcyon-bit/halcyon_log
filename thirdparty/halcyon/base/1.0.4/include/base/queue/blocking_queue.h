#ifndef BASE_BLOCKING_QUEUE_H
#define BASE_BLOCKING_QUEUE_H

#include <base/common/noncopyable.h>

#include <deque>
#include <mutex>
#include <chrono>
#include <cassert>
#include <condition_variable>

BASE_BEGIN_NAMESPACE

/**
 * @brief   带锁的队列(暂不支持多线程退出，需要多次唤醒)
 * @ps      每次 take(无超时) 必获取数据, 所以停止需要自定义规则
 */
template<typename T>
class BlockingQueue : noncopyable
{
public:
    BlockingQueue() = default;

    /**
     * @brief       入队
     * @param[in]   数据
     */
    void push(const T& x)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(x);
        }
        cv_.notify_one();
    }
    void push(T&& x)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::forward<T>(x));
        }
        cv_.notify_one();
    }

    /**
     * @brief   出队(若无则等待)
     * @return  数据
     */
    T take()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.empty()) {
            cv_.wait(lock);
        }
        assert(!queue_.empty());
        T front(std::move(queue_.front()));
        queue_.pop_front();
        return front;
    }

    /**
     * @brief       出队
     * @param[in]   超时时间(毫秒)
     * @param[out]  获取到的数据
     * @return      是否获取到数据
     */
    bool take(uint32_t millsec, T& data)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 超时等待(当有数据时或者超时，返回结果)
        // wait_for 返回 true 表示线程被唤醒, false 表示超时
        if (cv_.wait_for(lock, std::chrono::milliseconds(millsec), [this] { return !queue_.empty(); })) {
            data = std::move(queue_.front());
            queue_.pop_front();
            return true;
        } else {
            return false;
        }
    }

    /**
     * @brief   获取队列大小
     */
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * @brief   清空数据
     */
    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.clear();
    }

    /**
     * @brief   队列是否为空
     */
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    //! 数据队列
    std::deque<T> queue_;
};

BASE_END_NAMESPACE

#endif