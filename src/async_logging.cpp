#include "async_logging.h"
#include "log_define.h"
#include "log_file.h"
#include "base/utility/utility.h"

#include <cassert>
#include <memory>

// 最大日志文件大小（以MB为单位）,超过会对文件进行分割  FLAGS_max_log_size
DECLARE_uint32(max_log_size);

// 设置可以缓冲日志的最大秒数，FLAGS_log_flush_interval
DECLARE_uint32(log_flush_interval);

using namespace halcyon::log;

AsyncLogging::AsyncLogging(std::string_view filePrefix)
    : filePrefix_(filePrefix)
{
    currentBuffer_ = std::make_unique<Buffer>();
    nextBuffer_ = std::make_unique<Buffer>();
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16);
}

AsyncLogging::~AsyncLogging()
{
    if (running_) {
        stop();
    }
}

void AsyncLogging::start()
{
    running_ = true;
    thread_ = std::make_shared<std::thread>(&AsyncLogging::threadFunc, this);
}

void AsyncLogging::stop()
{
    running_ = false;
    cv_.notify_all();
    thread_->join();
}

void AsyncLogging::append(const char* logline, size_t len)
{
    /*
     * 若当前 buffer 未满，则写入当前 buffer。
     * 否则，将当前 buffer 放入 vector 中，唤醒线程，写入文件
     *     若 nextbuffer 不为空，curbuffer = move(nextbuffer)
     *     否则(nextbuffer 为空)，重新 new buffer
     */
    std::unique_lock<std::mutex> lock(mutex_);
    if (currentBuffer_->avail() <= len || (curLogLenght_ >> 20U >= maxLogSize())) {
        curLogLenght_ = (curLogLenght_ >> 20U >= maxLogSize()) ? 0 : curLogLenght_;
        buffers_.push_back(std::move(currentBuffer_));

        if (nextBuffer_ != nullptr) {
            currentBuffer_ = std::move(nextBuffer_);
        }
        else {
            currentBuffer_.reset(new Buffer);
        }
        cv_.notify_one();
    }
    currentBuffer_->append(logline, len);
    curLogLenght_ += static_cast<uint32_t>(len);
}

uint32_t AsyncLogging::maxLogSize()
{
    return (FLAGS_max_log_size > 0 && FLAGS_max_log_size < 4096 ? FLAGS_max_log_size : 1);
}

void AsyncLogging::threadFunc()
{
    /*
     *  总共有 4 个固定 buffer 用于保存日志，
     *  两个共外部使用，两个用于交换
     */
    assert(running_ == true);

    // 单线程写日志(即多个生产者，一个消费者)
    LogFileManager logFile(filePrefix_, false);

    // 两个内部 buffer 用于和类中成员变量的两个 buffer 交换
    BufferPtr newBuffer1 = std::make_unique<Buffer>();
    BufferPtr newBuffer2 = std::make_unique<Buffer>();

    newBuffer1->bzero();
    newBuffer2->bzero();

    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    while (running_) {
        assert(newBuffer1 && newBuffer1->length() == 0);
        assert(newBuffer2 && newBuffer2->length() == 0);
        assert(buffersToWrite.empty());

        {
            std::unique_lock<std::mutex> lock(mutex_);
            std::cv_status ret = std::cv_status::timeout;
            while (buffers_.empty()) {
                // 等待唤醒，或者超时
                ret = cv_.wait_for(lock, std::chrono::seconds(FLAGS_log_flush_interval));
            }
            if (ret == std::cv_status::timeout) {
                // 将当前 buffer 放入 buffers 中，启用备用 buffer
                // 交换 buffers
                buffers_.push_back(std::move(currentBuffer_));
                currentBuffer_ = std::move(newBuffer1);
            }
            buffersToWrite.swap(buffers_);
            if (nextBuffer_ == nullptr) {
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        assert(!buffersToWrite.empty());

        if (buffersToWrite.size() > 25) {
            // 当前时间
            std::string strTime = base::formatTime("%Y-%m-%d %H:%M:%S", ::time(nullptr));

            // 当日志量过大时，抛弃多余日志
            char buf[256]{ 0 };
            snprintf(buf, sizeof(buf), "Dropped log messages at %s, %zd larger buffers\n",
                strTime.c_str(), buffersToWrite.size() - 2);
            fputs(buf, stderr);

            logFile.append(buf, strlen(buf));
            buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
        }

        // 将日志全部写入文件
        for (const auto& buffer : buffersToWrite) {
            logFile.append(buffer->data(), buffer->length());
        }

        // 保留两个 buffer
        if (buffersToWrite.size() > 2) {
            buffersToWrite.resize(2);
        }

        // 恢复两个备用 buffer
        if (newBuffer1 == nullptr) {
            assert(!buffersToWrite.empty());
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        if (newBuffer2 == nullptr) {
            assert(!buffersToWrite.empty());
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        buffersToWrite.clear();
        logFile.flush();
    }
    logFile.flush();
}


#include "logging.h"
using halcyon::log::Logger;

static std::shared_ptr<halcyon::log::AsyncLogging> g_asyncLog;

namespace halcyon
{
    /*
     * @brief   异步日志输出
     */
    static void asyncOutput(const char* msg, size_t len)
    {
        g_asyncLog->append(msg, len);
    }

    /*
     * @brief       初始化日志模块
     * @param[in]   日志名前缀
     */
    void initLog(const char* logname)
    {
        if (g_asyncLog == nullptr) {
            auto logSPtr = std::make_shared<halcyon::log::AsyncLogging>(logname);
            g_asyncLog = logSPtr;
            logSPtr->start();
            Logger::setOutput(asyncOutput);
        }
    }

    /*
     * @brief   反初始化日志模块
     */
    void uninitLog()
    {
        if (g_asyncLog != nullptr) {
            g_asyncLog->stop();
            g_asyncLog.reset();
        }
    }
}