#ifndef LOG_LOG_FILE_H
#define LOG_LOG_FILE_H

#include <log/log_define.h>

#include <base/common/noncopyable.h>

#ifdef USE_HALCYON_STRING_VIEW
#include <base/string/string_view.h>
#define HALCYON_STRING_VIEW_NS  ::halcyon::base::
#else
#include <string_view>
#define HALCYON_STRING_VIEW_NS  ::std::
#endif

#include <list>
#include <mutex>

/// 日志文件的相关操作
LOG_BEGIN_NAMESPACE

/**
 * @brief   日志文件
 * @ps      写文件(非线程安全)
 */
class LogFile : base::noncopyable
{
public:
    /**
     * @brief       构造函数，打开文件
     * @param[in]   文件名
     */
    explicit LogFile(HALCYON_STRING_VIEW_NS string_view filename);

    /**
     * @brief   析构函数，关闭文件
     */
    ~LogFile();

    /**
     * @brief       向文件中追加信息，并不会直接写入文件，而是先写入缓冲区，之后再写入文件
     * @param[in]   信息
     * @param[in]   信息的长度
     */
    void append(HALCYON_STRING_VIEW_NS string_view logline);

    /**
     * @brief   将缓冲区的信息写入文件中
     */
    void flush();

    /**
     * @brief   获取已写入的字节数
     */
    size_t writtenBytes() const
    {
        return written_bytes_;
    }

private:
    /**
     * @brief       向缓冲区中写入信息
     * @param[in]   信息
     * @param[in]   信息的长度
     * @return      写入的字节数
     */
    size_t write(const char* logline, size_t len);

private:
    //! 文件指针
    FILE* fp_{ nullptr };
    //! 缓冲区
    char buffer_[1024 * 64];
    //! 当前写入长度
    size_t written_bytes_{ 0 };
};

/**
 * @brief   日志文件管理，主要是日志文件的滚动
 */
class LogFileManager : base::noncopyable
{
public:
    /**
     * @brief       构造函数
     * @param[in]   日志存放的文件夹
     * @param[in]   日志文件名前缀
     * @param[in]   单日志文件大小(KB)
     * @param[in]   总日志文件数量(超过覆盖最旧的)
     * @param[in]   flush 刷新时间间隔(s)
     * @param[in]   是否开启多线程模式（即对文件操作加锁）
     */
    LogFileManager(HALCYON_STRING_VIEW_NS string_view dir, HALCYON_STRING_VIEW_NS string_view prefix,
        size_t max_size, size_t max_file, size_t flush_interval = 3, bool thread_safe = true);
    /**
     * @brief   析构函数
     */
    ~LogFileManager();

    /**
     * @brief       将信息写入文件(缓冲区)中
     * @param[in]   信息
     * @param[in]   信息的长度
     */
    void append(HALCYON_STRING_VIEW_NS string_view logline);

    /**
     * @brief   刷新，将缓冲区的数据写入文件中
     */
    void flush();

private:
    /**
     * @brief   文件滚动，当写入字节大于 FLAGS_max_log_size，重新创建文件开始记录
     */
    void rollFile();

    /**
     * @brief   获取最大日志文件大小
     */
    size_t maxLogSize();

    /**
     * @brief       将信息写入文件(缓冲区)中
     * @param[in]   信息
     * @param[in]   信息的长度
     */
    void append_unlocked(HALCYON_STRING_VIEW_NS string_view logline);

    /**
     * @brief       获取文件名称，basename+时间+.log
     * @return      文件名
     */
    std::string genLogFileName();

private:
    //! 单日志文件大小
    const size_t max_size_;
    //! 日志文件数量上限
    const size_t max_file_;
    //! 日志刷新周期(s)
    const size_t flush_interval_;

    //! 当天时间戳
    time_t start_of_time_{ 0 };
    //! 上次刷新时间
    time_t last_flush_{ 0 };
    //! 当前文件数量
    size_t cur_file_count_{ 0 };

    //! 日志文件前缀
    std::string prefix_;

    //! 是否加锁，保证线程安全
    const bool thread_safe_;
    std::mutex mutex_;

    //! 文件操作
    std::unique_ptr<LogFile> file_;

    //! 历史文件名
    std::list<std::string> names_;
};

LOG_END_NAMESPACE

#endif