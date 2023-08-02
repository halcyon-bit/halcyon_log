#include "logging.h"
#include "base/common/platform.h"

#include <thread>
#include <cassert>

#define DEFINE_VARIABLE(type, name, value)              \
    namespace halcyon::log {                            \
        HALCYON_LOG_DLL_DECL type FLAGS_##name(value);  \
    }                                                   \
    using halcyon::log::FLAGS_##name

#define DEFINE_bool(name, value)            \
    DEFINE_VARIABLE(bool, name, value)

#define DEFINE_int32(name, value)           \
    DEFINE_VARIABLE(int32_t, name, value)   \

#define DEFINE_uint32(name, value)          \
    DEFINE_VARIABLE(uint32_t, name, value)

#define DEFINE_string(name, value)                                              \
    namespace halcyon::log {                                                    \
        std::string FLAGS_##name##_buf(value);                                  \
        HALCYON_LOG_DLL_DECL std::string& FLAGS_##name = FLAGS_##name##_buf;    \
    }                                                                           \
    using halcyon::log::FLAGS_##name

DEFINE_bool(log_stderr, true);

DEFINE_int32(min_log_level, 0);

DEFINE_uint32(max_log_size, 10);

DEFINE_uint32(log_flush_interval, 3);

// 日志目录
#ifdef WINDOWS
DEFINE_string(log_dir, R"(.\log\)");
#elif defined LINUX
DEFINE_string(log_dir, R"(./log/)");
#endif

void COLOR_PRINT(const char* s, int color);

namespace halcyon::log
{
    thread_local char g_time[64];  // 时间字符串(线程独有)
    thread_local long long g_lastSecond;  // 上次时间(s)(线程独有)

    const char* logLevelName[Logger::NUM_LOG_LEVELS] = {
        "TRACE ",
        "DEBUG ",
        "INFO  ",
        "WARN  ",
        "ERROR ",
        "FATAL ",
    };

    // helper class for known string length at compile time
    class T
    {
    public:
        T(const char* str, size_t len)
            : str_(str), len_(len)
        {
            assert(strlen(str) == len);
        }

        const char* str_;  // 字符串
        const size_t len_;  // 长度
    };

    inline LogStream& operator<<(LogStream& s, T v)
    {
        s.append(v.str_, v.len_);
        return s;
    }

    inline LogStream& operator<<(LogStream& s, const Logger::SourceFile& v)
    {
        s.append(v.data_, v.size_);
        return s;
    }

    void defaultOutput(const char* msg, size_t len)
    {
        //size_t n = fwrite(msg, 1, len, stdout);
        //(void)n;
    }

    void defaultFlush()
    {
        fflush(stdout);
    }

    Logger::OutputFunc g_output = defaultOutput;  // 输出函数
    Logger::FlushFunc g_flush = defaultFlush;  // 刷新函数

    Logger::Impl::Impl(LogLevel level, const SourceFile& file, int line)
        : time_(system_clock::now()), stream_(), level_(level), line_(line)
        , basename_(file)
    {
        formatTime();
        stream_ << T(logLevelName[level], 6);
    }

    void Logger::Impl::formatTime()
    {
        long long second = duration_cast<seconds>(time_.time_since_epoch()).count();
        long long milliSecond = duration_cast<milliseconds>(time_.time_since_epoch()).count();

        if (second != g_lastSecond) {
            g_lastSecond = second;
            struct tm tmNow;
#ifdef WINDOWS
            localtime_s(&tmNow, &second);
#elif defined LINUX
            localtime_r((time_t*)&second, &tmNow);
#endif
            int len = snprintf(g_time, sizeof(g_time), "%4d%02d%02d %02d:%02d:%02d",
                tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
                tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
            assert(len == 17);
            (void)len;
        }
        // 毫秒
        auto ms = duration_cast<milliseconds>(time_.time_since_epoch()) - duration_cast<seconds>(time_.time_since_epoch());

        Fmt us(".%03d ", ms.count());
        assert(us.length() == 5);
        stream_ << T(g_time, 17) << T(us.data(), us.length());
    }

    void Logger::Impl::finish()
    {
        stream_ << " - " << basename_ << ':' << line_ << '\n';
    }

    Logger::Logger(SourceFile file, int line)
        : impl_(LOG_LEVEL_INFO, file, line)
    {}

    Logger::Logger(SourceFile file, int line, LogLevel level, const char* func)
        : impl_(level, file, line)
    {
        impl_.stream_ << "[" << func << "] ";
    }

    Logger::Logger(SourceFile file, int line, LogLevel level)
        : impl_(level, file, line)
    {}

    Logger::Logger(SourceFile file, int line, bool toAbort)
        : impl_(toAbort ? LOG_LEVEL_FATAL : LOG_LEVEL_ERROR, file, line)
    {}

    Logger::~Logger()
    {
        impl_.finish();
        const LogStream::Buffer& buf(stream().buffer());
        g_output(buf.data(), buf.length());

        if (FLAGS_log_stderr) {
            switch (impl_.level_) {
            case LOG_LEVEL_INFO:
                COLOR_PRINT(buf.data(), 1);
                break;

            case LOG_LEVEL_ERROR:
                COLOR_PRINT(buf.data(), 4);
                break;

            case LOG_LEVEL_WARN:
                COLOR_PRINT(buf.data(), 6);
                break;

            default:
                COLOR_PRINT(buf.data(), 7);
                break;
            }
        }

        if (impl_.level_ == LOG_LEVEL_FATAL)
        {
            g_flush();
            abort();
        }
    }

    void Logger::setOutput(OutputFunc out)
    {
        g_output = out;
    }

    void Logger::setFlush(FlushFunc flush)
    {
        g_flush = flush;
    }
}

#if defined WINDOWS
#include <windows.h>

// 0 = 黑色 8 = 灰色
// 1 = 蓝色 9 = 淡蓝色
// 2 = 绿色 10 = 淡绿色
// 3 = 浅绿色 11 = 淡浅绿色
// 4 = 红色 12 = 淡红色
// 5 = 紫色 13 = 淡紫色
// 6 = 黄色 14 = 淡黄色
// 7 = 白色 15 = 亮白色
void COLOR_PRINT(const char* s, int color)
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY | color);
    printf(s);
    SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY | 7);
}
#elif defined LINUX
// Black       0;30     Dark Gray     1;30
// Blue        0;34     Light Blue    1;34
// Green       0;32     Light Green   1;32
// Cyan        0;36     Light Cyan    1;36
// Red         0;31     Light Red     1;31
// Purple      0;35     Light Purple  1;35
// Brown       0;33     Yellow        1;33
// Light Gray  0;37     White         1;37
void COLOR_PRINT(const char* s, int color)
{
    // \033[0m 是恢复颜色到默认状态
    switch (color) {
    case 1:  // 蓝色
        printf("\033[0;34m%s\033[0m", s);
        break;
    case 4:  // 红色
        printf("\033[0;31m%s\033[0m", s);
        break;
    case 6:  // 黄色
        printf("\033[1;33m%s\033[0m", s);
        break;
    case 7:  // 白色
        printf("\033[1;37m%s\033[0m", s);
        break;
    }
}
#endif