#include "log_stream.h"
#include "base/common/platform.h"

#include <cinttypes>
#include <cassert>
#ifdef WINDOWS
#else
#include <sstream>
#endif
#include <algorithm>

namespace halcyon::log
{
    namespace detail
    {
        // 十进制数字
        constexpr char digits[] = "9876543210123456789";
        const char* zero = digits + 9;
        static_assert(sizeof(digits) == 20, "wrong number of digits");

        // 16进制数字
        constexpr char digitsHex[] = "0123456789ABCDEF";
        static_assert(sizeof(digitsHex) == 17, "wrong number of digitsHex");

        /*
         * @brief		整数转为字符串
         * @param[out]	字符串，用于存贮结果
         * @param[in]	整数值
         * @return		字符串的长度
         */
        template<typename T>
        size_t convert(char buf[], T value)
        {
            T i = value;
            char* p = buf;

            do {
                int lsd = static_cast<int>(i % 10);
                i /= 10;
                *p++ = zero[lsd];
            } while (i != 0);

            if (value < 0) {
                *p++ = '-';
            }
            *p = '\0';
            std::reverse(buf, p);
            return p - buf;
        }

        /*
         * @brief		整数转为十六进制字符串
         * @param[out]	十六进制字符串，用于存贮结果
         * @param[in]	整数值
         * @return		字符串的长度
         */
        size_t convertHex(char buf[], uintptr_t value)
        {
            uintptr_t i = value;
            char* p = buf;

            do {
                int lsd = static_cast<int>(i % 16);
                i /= 16;
                *p++ = digitsHex[lsd];
            } while (i != 0);

            *p = '\0';
            std::reverse(buf, p);
            return p - buf;
        }

        template class FixedBuffer<kSmallBuffer>;
        template class FixedBuffer<kLargeBuffer>;

        template<size_t SIZE>
        const char* FixedBuffer<SIZE>::debugString()
        {
            *cur_ = '\0';
            return data_;
        }

        template<size_t SIZE>
        void FixedBuffer<SIZE>::cookieStart()
        {}

        template<size_t SIZE>
        void FixedBuffer<SIZE>::cookieEnd()
        {}
    }
}

using namespace halcyon::log;

/*
 * Format a number with 5 characters, including SI units.
 * [0,     999]
 * [1.00k, 999k]
 * [1.00M, 999M]
 * [1.00G, 999G]
 * [1.00T, 999T]
 * [1.00P, 999P]
 * [1.00E, inf)
 */
std::string formatSI(long long s)
{
    double n = static_cast<double>(s);
    char buf[64];
    if (s < 1000)
        snprintf(buf, sizeof(buf), "%" PRId64, s);
    else if (s < 9995)
        snprintf(buf, sizeof(buf), "%.2fk", n / 1e3);
    else if (s < 99950)
        snprintf(buf, sizeof(buf), "%.1fk", n / 1e3);
    else if (s < 999500)
        snprintf(buf, sizeof(buf), "%.0fk", n / 1e3);
    else if (s < 9995000)
        snprintf(buf, sizeof(buf), "%.2fM", n / 1e6);
    else if (s < 99950000)
        snprintf(buf, sizeof(buf), "%.1fM", n / 1e6);
    else if (s < 999500000)
        snprintf(buf, sizeof(buf), "%.0fM", n / 1e6);
    else if (s < 9995000000)
        snprintf(buf, sizeof(buf), "%.2fG", n / 1e9);
    else if (s < 99950000000)
        snprintf(buf, sizeof(buf), "%.1fG", n / 1e9);
    else if (s < 999500000000)
        snprintf(buf, sizeof(buf), "%.0fG", n / 1e9);
    else if (s < 9995000000000)
        snprintf(buf, sizeof(buf), "%.2fT", n / 1e12);
    else if (s < 99950000000000)
        snprintf(buf, sizeof(buf), "%.1fT", n / 1e12);
    else if (s < 999500000000000)
        snprintf(buf, sizeof(buf), "%.0fT", n / 1e12);
    else if (s < 9995000000000000)
        snprintf(buf, sizeof(buf), "%.2fP", n / 1e15);
    else if (s < 99950000000000000)
        snprintf(buf, sizeof(buf), "%.1fP", n / 1e15);
    else if (s < 999500000000000000)
        snprintf(buf, sizeof(buf), "%.0fP", n / 1e15);
    else
        snprintf(buf, sizeof(buf), "%.2fE", n / 1e18);
    return buf;
}

/*
 * [0, 1023]
 * [1.00Ki, 9.99Ki]
 * [10.0Ki, 99.9Ki]
 * [ 100Ki, 1023Ki]
 * [1.00Mi, 9.99Mi]
 */
std::string formatIEC(long long s)
{
    double n = static_cast<double>(s);
    char buf[64];
    const double Ki = 1024.0;
    const double Mi = Ki * 1024.0;
    const double Gi = Mi * 1024.0;
    const double Ti = Gi * 1024.0;
    const double Pi = Ti * 1024.0;
    const double Ei = Pi * 1024.0;

    if (n < Ki)
        snprintf(buf, sizeof buf, "%" PRId64, s);
    else if (n < Ki * 9.995)
        snprintf(buf, sizeof buf, "%.2fKi", n / Ki);
    else if (n < Ki * 99.95)
        snprintf(buf, sizeof buf, "%.1fKi", n / Ki);
    else if (n < Ki * 1023.5)
        snprintf(buf, sizeof buf, "%.0fKi", n / Ki);

    else if (n < Mi * 9.995)
        snprintf(buf, sizeof buf, "%.2fMi", n / Mi);
    else if (n < Mi * 99.95)
        snprintf(buf, sizeof buf, "%.1fMi", n / Mi);
    else if (n < Mi * 1023.5)
        snprintf(buf, sizeof buf, "%.0fMi", n / Mi);

    else if (n < Gi * 9.995)
        snprintf(buf, sizeof buf, "%.2fGi", n / Gi);
    else if (n < Gi * 99.95)
        snprintf(buf, sizeof buf, "%.1fGi", n / Gi);
    else if (n < Gi * 1023.5)
        snprintf(buf, sizeof buf, "%.0fGi", n / Gi);

    else if (n < Ti * 9.995)
        snprintf(buf, sizeof buf, "%.2fTi", n / Ti);
    else if (n < Ti * 99.95)
        snprintf(buf, sizeof buf, "%.1fTi", n / Ti);
    else if (n < Ti * 1023.5)
        snprintf(buf, sizeof buf, "%.0fTi", n / Ti);

    else if (n < Pi * 9.995)
        snprintf(buf, sizeof buf, "%.2fPi", n / Pi);
    else if (n < Pi * 99.95)
        snprintf(buf, sizeof buf, "%.1fPi", n / Pi);
    else if (n < Pi * 1023.5)
        snprintf(buf, sizeof buf, "%.0fPi", n / Pi);

    else if (n < Ei * 9.995)
        snprintf(buf, sizeof buf, "%.2fEi", n / Ei);
    else
        snprintf(buf, sizeof buf, "%.1fEi", n / Ei);
    return buf;
}

LogStream& LogStream::operator<<(short n)
{
    *this << static_cast<int>(n);
    return *this;
}

LogStream& LogStream::operator<<(unsigned short n)
{
    *this << static_cast<unsigned int>(n);
    return *this;
}

LogStream& LogStream::operator<<(int n)
{
    formatInteger(n);
    return *this;
}

LogStream& LogStream::operator<<(unsigned int n)
{
    formatInteger(n);
    return *this;
}

LogStream& LogStream::operator<<(long n)
{
    formatInteger(n);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long n)
{
    formatInteger(n);
    return *this;
}

LogStream& LogStream::operator<<(long long n)
{
    formatInteger(n);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long long n)
{
    formatInteger(n);
    return *this;
}

LogStream& LogStream::operator<<(const void* p)
{
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    if (buffer_.avail() >= kMaxNumericSize) {
        char* buf = buffer_.current();
        buf[0] = '0';
        buf[1] = 'x';
        size_t len = detail::convertHex(buf + 2, v);
        buffer_.add(len + 2);
    }
    return *this;
}

LogStream& LogStream::operator<<(double d)
{
    if (buffer_.avail() >= kMaxNumericSize) {
        int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12g", d);
        buffer_.add(len);
    }
    return *this;
}

LogStream& LogStream::operator<<(const std::thread::id& threadId)
{
#ifdef WINDOWS
    // 不一定对(可能与版本有关)
    // std::thread::id 内部就一个 unsigned int 成员变量
    _Thrd_id_t t = *(_Thrd_id_t*)&threadId;
    *this << t;
#else
    std::stringstream sstream;
    sstream << threadId;
    buffer_.append(sstream.str().c_str(), sstream.str().length());
#endif
    return *this;
}

void LogStream::staticCheck()
{
    static_assert(kMaxNumericSize - 10 > std::numeric_limits<double>::digits10,
        "kMaxNumericSize is large enough");
    static_assert(kMaxNumericSize - 10 > std::numeric_limits<long double>::digits10,
        "kMaxNumericSize is large enough");
    static_assert(kMaxNumericSize - 10 > std::numeric_limits<long>::digits10,
        "kMaxNumericSize is large enough");
    static_assert(kMaxNumericSize - 10 > std::numeric_limits<long long>::digits10,
        "kMaxNumericSize is large enough");
}

template<typename T>
void LogStream::formatInteger(T v)
{
    if (buffer_.avail() >= kMaxNumericSize) {
        size_t len = detail::convert(buffer_.current(), v);
        buffer_.add(len);
    }
}

template<typename T>
Fmt::Fmt(const char* fmt, T val)
{
    static_assert(std::is_arithmetic<T>::value == true, "Must be arithmetic type");

    length_ = snprintf(buffer_, sizeof(buffer_), fmt, val);
    assert(length_ < sizeof(buffer_));
}

// 全特化(定义可能用到的模板参数，这样模板的实现就可以放在 cpp 中)
template Fmt::Fmt(const char* fmt, char);
template Fmt::Fmt(const char* fmt, short);
template Fmt::Fmt(const char* fmt, unsigned short);
template Fmt::Fmt(const char* fmt, int);
template Fmt::Fmt(const char* fmt, unsigned int);
template Fmt::Fmt(const char* fmt, long);
template Fmt::Fmt(const char* fmt, unsigned long);
template Fmt::Fmt(const char* fmt, long long);
template Fmt::Fmt(const char* fmt, unsigned long long);
template Fmt::Fmt(const char* fmt, float);
template Fmt::Fmt(const char* fmt, double);
