#ifndef BASE_ELAPSED_TIMER_H
#define BASE_ELAPSED_TIMER_H

#include <base/common/noncopyable.h>

#include <chrono>

BASE_BEGIN_NAMESPACE

/// 计时器
class ElapsedTimer final : noncopyable
{
public:
    ElapsedTimer()
        : begin_(std::chrono::high_resolution_clock::now())
    {}

    /**
     * @brief   重置
     */
    void reset()
    {
        begin_ = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief   默认输出毫秒
     */
    template<typename Duration = std::chrono::milliseconds>
    int64_t elapsed() const
    {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<Duration>(end - begin_).count();
    }

    /**
     * @brief   微秒
     */
    int64_t elapsedMicro() const
    {
        return elapsed<std::chrono::microseconds>();
    }

    /**
     * @brief   纳秒
     */
    int64_t elapsedNano() const
    {
        return elapsed<std::chrono::nanoseconds>();
    }

    /**
     * @brief   秒
     */
    int64_t elapsedSecond() const
    {
        return elapsed<std::chrono::seconds>();
    }

    /**
     * @brief   分钟
     */
    int64_t elapsedMinute() const
    {
        return elapsed<std::chrono::minutes>();
    }

    /**
     * @brief   小时
     */
    int64_t elapsedHour() const
    {
        return elapsed<std::chrono::hours>();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> begin_;
};

BASE_END_NAMESPACE

#endif