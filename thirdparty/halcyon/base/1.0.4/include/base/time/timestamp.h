#ifndef BASE_TIMESTAMP_H
#define BASE_TIMESTAMP_H

#include <base/common/base_define.h>

#include <chrono>
#include <string>
#include <thread>
#include <cinttypes>

BASE_BEGIN_NAMESPACE

using Microsecond = std::chrono::microseconds;
using Millisecond = std::chrono::milliseconds;
using Second = std::chrono::seconds;

/// 微妙级别
using Timestamp = std::chrono::time_point<std::chrono::system_clock, Microsecond>;

/// 每秒对应的微秒数、毫秒数
static constexpr int64_t kMicroSecondsPerSecond = 1000 * 1000;
static constexpr int64_t kMilliSecondsPerSecond = 1000;

/**
 * @brief   获取当前时间
 */
inline Timestamp now()
{
    return std::chrono::time_point_cast<Microsecond>(std::chrono::system_clock::now());
}

/**
 * @brief   获取微妙数
 */
inline int64_t microSecondsSinceEpoch(const Timestamp& time)
{
    return time.time_since_epoch().count();
}

/**
 * @brief   获取毫秒数
 */
inline int64_t milliSecondsSinceEpoch(const Timestamp& time)
{
    return microSecondsSinceEpoch(time) / kMilliSecondsPerSecond;
}

/**
 * @brief   获取秒数
 */
inline time_t secondsSinceEpoch(const Timestamp& time)
{
    return static_cast<time_t>(microSecondsSinceEpoch(time) / kMicroSecondsPerSecond);
}

/**
 * @brief   休眠(ms)
 */
inline void sleep(uint64_t milli)
{
    std::this_thread::sleep_for(Millisecond(milli));
}

/**
 * @brief   时间是否有效
 */
inline bool isValid(const Timestamp& time)
{
    return microSecondsSinceEpoch(time) != 0;
}

/**
 * @brief       转化为字符串(秒数.微秒数)
 * @param[in]   时间
 */
inline std::string toString(const Timestamp& time)
{
    char buf[32]{ 0 };
    int64_t micros = microSecondsSinceEpoch(time);
    int64_t second = micros / kMicroSecondsPerSecond;
    int64_t microseconds = micros % kMicroSecondsPerSecond;
    snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 "", second, microseconds);
    return buf;
}

/**
 * @brief       时间格式化(字符串)
 * @param[in]   时间
 * @param[in]   是否显示微妙
 * @return      "20200108 14:58:40.000258"
 */
inline std::string toFormatString(const Timestamp& time, bool show_microseconds = true)
{
    int64_t micros = microSecondsSinceEpoch(time);
    int64_t seconds = static_cast<time_t>(micros / kMicroSecondsPerSecond);

    struct tm tm_time;
#ifdef WINDOWS
    localtime_s(&tm_time, &seconds);
#elif defined LINUX
    localtime_r(&seconds, &tm_time);
#endif

    char buf[64] = { 0 };
    if (show_microseconds) {
        int32_t microseconds = static_cast<int32_t>(micros % kMicroSecondsPerSecond);
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
            tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
            microseconds);
    } else {
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
            tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }
    return buf;
}

/**
 * @brief   重载操作符
 */
inline bool operator<(Timestamp lhs, Timestamp rhs)
{
    return microSecondsSinceEpoch(lhs) < microSecondsSinceEpoch(rhs);
}
inline bool operator>(Timestamp lhs, Timestamp rhs)
{
    return (rhs < lhs);
}

inline bool operator<=(Timestamp lhs, Timestamp rhs)
{
    return !(lhs > rhs);
}
inline bool operator>=(Timestamp lhs, Timestamp rhs)
{
    return !(lhs < rhs);
}

inline bool operator==(Timestamp lhs, Timestamp rhs)
{
    return microSecondsSinceEpoch(lhs) == microSecondsSinceEpoch(rhs);
}
inline bool operator!=(Timestamp lhs, Timestamp rhs)
{
    return !(lhs == rhs);
}


/**
 * @brief   增加时间(ms)
 */
inline Timestamp addTime(Timestamp timestamp, double milli)
{
    return timestamp + Microsecond(static_cast<int64_t>(milli) * kMilliSecondsPerSecond);
}

/**
 * @brief   减少时间(ms)
 */
inline Timestamp decTime(Timestamp timestamp, double milli)
{
    return timestamp - Microsecond(static_cast<int64_t>(milli) * kMilliSecondsPerSecond);
}

/**
 * @brief   时间差值(ms)
 */
inline double timeDifference(Timestamp high, Timestamp low)
{
    int64_t diff = microSecondsSinceEpoch(high) - microSecondsSinceEpoch(low);
    return static_cast<double>(diff) / kMilliSecondsPerSecond;
}

BASE_END_NAMESPACE

#endif