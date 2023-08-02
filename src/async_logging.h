#pragma once

#include "base/common/noncopyable.h"
#include "log_stream.h"

#include <vector>
#include <atomic>
#include <string>
#include <string_view>
#include <condition_variable>

namespace halcyon::log
{
    /*
     * @brief   异步日志
     */
    class AsyncLogging : base::noncopyable
    {
    public:
        /*
         * @brief       构造函数
         * @param[in]   文件名前缀
         */
        AsyncLogging(std::string_view filePrefix);

        /*
         * @brief       析构
         */
        ~AsyncLogging();

        /*
         * @brief       添加日志记录
         * @param[in]   日志信息
         * @param[in]   信息长度
         */
        void append(const char* logline, size_t len);

        /*
         * @brief   启动日志功能
         */
        void start();

        /*
         * @brief   关闭日志功能
         */
        void stop();

    private:
        /*
         * @brief   获取日志滚动大小
         */
        uint32_t maxLogSize();

        /*
         * @brief   处理日志的线程
         */
        void threadFunc();

    private:
        using Buffer = detail::FixedBuffer<detail::kLargeBuffer>;
        using BufferPtr = std::unique_ptr<Buffer>;
        using BufferVector = std::vector<BufferPtr>;

        std::atomic<bool> running_{ false };  // 启动标志
        const std::string filePrefix_;  // 日志文件名称前缀

        std::shared_ptr<std::thread> thread_;  // 日志线程
        std::mutex mutex_;  // 锁
        std::condition_variable cv_;  // 条件变量

        BufferPtr currentBuffer_;  // 日志 buffer1
        BufferPtr nextBuffer_;  // 日志 buffer2 备用
        BufferVector buffers_;  // 保存写满的 buffer，异步写入文件中
        uint32_t curLogLenght_{ 0 };  // 当前日志长度(用于日志滚动)
    };
}