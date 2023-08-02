#ifndef LOG_LOG_FILE_H
#define LOG_LOG_FILE_H

#include <log/log_define.h>

#include <base/common/noncopyable.h>

#ifdef USE_HALCYON_STRING_VIEW
#include <base/string/string_view.h>
#else
#include <string_view>
#endif

#include <mutex>
#include <string>
#include <memory>

/// 日志文件的相关操作
LOG_BEGIN_NAMESPACE

/*
 * @brief   日志文件
 * @ps      写文件(非线程安全)
 */
class LogFile : base::noncopyable
{
public:
    /*
     * @brief       构造函数，打开文件
     * @param[in]   文件名
     */
    explicit LogFile(std::string_view filename);

    /*
     * @brief   析构函数，关闭文件
     */
    ~LogFile();

    /*
     * @brief       向文件中追加信息，并不会直接写入文件，而是先写入缓冲区，之后再写入文件
     * @param[in]   信息
     * @param[in]   信息的长度
     */
    void append(const char* logline, size_t len);

    /*
     * @brief   将缓冲区的信息写入文件中
     */
    void flush();

    /*
     * @brief   获取已写入的字节数
     */
    size_t writtenBytes() const
    {
        return writtenBytes_;
    }

private:
    /*
     * @brief       向缓冲区中写入信息
     * @param[in]   信息
     * @param[in]   信息的长度
     * @return      写入的字节数
     */
    size_t write(const char* logline, const size_t len);

private:
    FILE* fp_;  // 文件指针
    char buffer_[1024 * 64];  // 缓冲区
    size_t writtenBytes_;  // 写入长度
};

/*
    * @brief   日志文件管理，主要是日志文件的滚动
    */
class LogFileManager : base::noncopyable
{
public:
    /*
        * @brief       构造函数
        * @param[in]   日志文件名
        * @param[in]   是否开启多线程模式（即对文件操作加锁）
        */
    LogFileManager(const std::string& namePrefix, bool threadSafe = true);

    /*
        * @brief   析构函数
        */
    ~LogFileManager();

    /*
        * @brief       将信息写入文件(缓冲区)中
        * @param[in]   信息
        * @param[in]   信息的长度
        */
    void append(const char* logline, size_t len);

    /*
        * @brief   刷新，将缓冲区的数据写入文件中
        */
    void flush();

private:
    /*
        * @brief   文件滚动，当写入字节大于 FLAGS_max_log_size，重新创建文件开始记录
        */
    void rollFile();

    /*
        * @brief   获取最大日志文件大小
        */
    uint32_t maxLogSize();

    /*
        * @brief       将信息写入文件(缓冲区)中
        * @param[in]   信息
        * @param[in]   信息的长度
        */
    void append_unlocked(const char* logline, const size_t len);

    /*
        * @brief       获取文件名称，basename+时间+.log
        * @return      文件名
        */
    std::string getLogFileName();

private:
    const std::string namePrefix_;  // 文件名前缀

    bool threadSafe_;  // 是否加锁
    std::mutex mutex_;

    std::unique_ptr<LogFile> file_;  // 文件
};

LOG_END_NAMESPACE

#endif