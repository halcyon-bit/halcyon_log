#pragma once

#include "log_define.h"
#include "log_stream.h"

#include <chrono>
using namespace std::chrono;

// 日志消息除了日志文件之外是否输出到标准输出  FLAGS_also_log_to_stderr
DECLARE_bool(also_log_to_stderr);

// 
DECLARE_bool(colorlogtostderr);

// FLAGS_min_log_level
// 最小处理日志的级别(默认为 TRACE)  FLAGS_min_log_level
DECLARE_int32(min_log_level);

// FLAGS_max_log_size
// 最大日志文件大小（以MB为单位）,超过会对文件进行分割(默认为 10)
DECLARE_uint32(max_log_size);

// FLAGS_log_dir
// 日志保存目录(默认为软件运行根目录下的 log 文件夹), 若文件夹不存在则自动创建
DECLARE_string(log_dir);

namespace halcyon::log
{
#ifdef WINDOWS
#define FOLDER_SEPERATOR    '\\'
#elif defined LINUX
#define FOLDER_SEPERATOR    '/'
#endif
    /*
     * @brief   记录日志，在构造时，写入buffer中，析构时输出
     */
    class HALCYON_LOG_DLL_DECL Logger
    {
    public:
        /*
         * @brief   日志级别
         */
        enum LogLevel
        {
            LOG_LEVEL_TRACE,
            LOG_LEVEL_DEBUG,
            LOG_LEVEL_INFO,
            LOG_LEVEL_WARN,
            LOG_LEVEL_ERROR,
            LOG_LEVEL_FATAL,
            NUM_LOG_LEVELS,
        };

        /*
         * @brief   定位源文件名称
         * @ps      文件名是编译时确定的
         */
        class SourceFile
        {
        public:
            template<int N>
            SourceFile(const char(&arr)[N])
                : data_(arr), size_(N - 1)
            {
                const char* slash = strrchr(data_, FOLDER_SEPERATOR);
                if (slash != nullptr) {
                    data_ = slash + 1;
                    size_ -= static_cast<int>(data_ - arr);
                }
            }

            explicit SourceFile(const char* filename)
                : data_(filename)
            {
                const char* slash = strrchr(data_, FOLDER_SEPERATOR);
                if (slash != nullptr) {
                    data_ = slash + 1;
                }
                size_ = static_cast<int>(strlen(data_));
            }

            const char* data_;  // 文件名
            int size_;  // 长度
        };

        /*
         * @brief       构造函数
         * @param[in]   file(源文件名)
         * @param[in]   line(行号)
         * @param[in]   level(日志级别)
         * @param[in]   func(函数名)
         */
        Logger(SourceFile file, int line);
        Logger(SourceFile file, int line, LogLevel level);
        Logger(SourceFile file, int line, LogLevel level, const char* func);
        Logger(SourceFile file, int line, bool toAbort);
        ~Logger();

        /*
         * @brief   获取日志流
         */
        LogStream& stream() {
            return impl_.stream_;
        }

        /*
         * @brief   日志的输出函数
         */
        using OutputFunc = void(*)(const char* msg, size_t len);
        /*
         * @brief   刷新
         */
        using FlushFunc = void(*)();

        /*
         * @brief   设置相应的函数
         */
        static void setOutput(OutputFunc);
        static void setFlush(FlushFunc);

    private:
        // 实现
        class Impl
        {
        public:
            using LogLevel = Logger::LogLevel;

            /*
             * @brief       构造函数
             * @param[in]   级别
             * @param[out]  源文件
             * @param[out]  行号
             */
            Impl(LogLevel level, const SourceFile& file, int line);

            /*
             * @brief       格式化时间
             */
            void formatTime();

            /*
             * @brief       写入源文件和行号
             */
            void finish();

        public:
            time_point<system_clock> time_;  // 时间戳
            LogStream stream_;  // 日志流
            LogLevel level_;  // 某条日志级别
            int line_;  // 行号
            SourceFile basename_;  // 源文件
        };

        Impl impl_;
    };

    /*
     * @brief       检测 ptr 是否为空，为空则中止
     */
    template<typename T>
    T* checkNotNull(Logger::SourceFile file, int line, const char* names, T* ptr)
    {
        if (ptr == NULL) {
            Logger(file, line, Logger::LOG_LEVEL_FATAL).stream() << names;
        }
        return ptr;
    }
}

// TRACE
#define LOG_TRACE   if (FLAGS_min_log_level <= halcyon::log::Logger::LOG_LEVEL_TRACE) \
    halcyon::log::Logger(__FILE__, __LINE__, halcyon::log::Logger::LOG_LEVEL_TRACE, __FUNCTION__).stream()

// DEBUG
#define LOG_DEBUG   if (FLAGS_min_log_level <= halcyon::log::Logger::LOG_LEVEL_DEBUG) \
    halcyon::log::Logger(__FILE__, __LINE__, halcyon::log::Logger::LOG_LEVEL_DEBUG, __FUNCTION__).stream()

// INFO
#define LOG_INFO    if (FLAGS_min_log_level <= halcyon::log::Logger::LOG_LEVEL_INFO) \
    halcyon::log::Logger(__FILE__, __LINE__).stream()

// WARN
#define LOG_WARN    halcyon::log::Logger(__FILE__, __LINE__, halcyon::log::Logger::LOG_LEVEL_WARN).stream()

// ERROR
#define LOG_ERROR       halcyon::log::Logger(__FILE__, __LINE__, halcyon::log::Logger::LOG_LEVEL_ERROR).stream()
#define LOG_FATAL       halcyon::log::Logger(__FILE__, __LINE__, halcyon::log::Logger::LOG_LEVEL_FATAL).stream()
#define LOG_SYSERR      halcyon::log::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL    halcyon::log::Logger(__FILE__, __LINE__, true).stream()

// Check that the input is non NULL.  This very useful in constructor
// initializer lists.
#define CHECK_NOTNULL(val) \
    halcyon::log::checkNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

namespace halcyon
{
    /*
     * @brief       初始化日志模块(记录到本地(.\log\))
     * @param[in]   日志的输出名(文件名前缀)
     * @param[in]   是否开启控制台输出
     * @param[in]   日志级别
     */
    HALCYON_LOG_DLL_DECL void initLog(const char* logname);

    /*
     * @brief       反初始化
     */
    HALCYON_LOG_DLL_DECL void uninitLog();
}