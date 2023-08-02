#pragma once
#include "log_define.h"
#include "base/common/noncopyable.h"

#include <string>
#include <thread>
#include <cstring>
#include <string_view>  // 可以提高性能(C++17) 无字符拷贝

namespace halcyon::log
{
    namespace detail
    {
        constexpr size_t kSmallBuffer = 4000;
        constexpr size_t kLargeBuffer = 4000 * 1000;

        /*
         * @brief       缓冲区
         * @param[in]   缓冲区大小
         */
        template <size_t SIZE>
        class FixedBuffer : base::noncopyable
        {
        public:
            using cookieFunc = void(*)();

            /*
             * @brief   构造函数
             */
            FixedBuffer() : cur_(data_)
            {
                setCookie(cookieStart);
            }

            /*
             * @brief   析构函数
             */
            ~FixedBuffer()
            {
                setCookie(cookieEnd);
            }

            /*
             * @brief       添加数据到缓冲区中
             * @param[in]   数据内容
             * @param[in]   长度
             */
            void append(const char* buf, const size_t len)
            {
                if (avail() > len) {
                    memcpy(cur_, buf, len);
                    cur_ += len;
                }
            }

            /*
             * @brief   获取缓冲区中的数据
             * @return  数据
             */
            const char* data() const
            {
                return data_;
            }

            /*
             * @brief   获取当前缓冲区的实际数据长度
             * @return  已写入的数据长度
             */
            size_t length() const
            {
                return static_cast<size_t>(cur_ - data_);
            }

            /*
             * @brief   获取缓冲区指针的当前位置，即空闲空间的开始
             * @return  当前位置
             */
            char* current()
            {
                return cur_;
            }

            /*
             * @brief   获取缓冲区剩余大小
             * @return  缓冲区的剩余大小
             */
            size_t avail() const
            {
                return static_cast<size_t>(end() - cur_);
            }

            /*
             * @brief       增加内容，但没有写入字符
             * @param[in]   内容长度
             */
            void add(const size_t len)
            {
                cur_ += len;
            }

            /*
             * @brief   重置缓冲区，即清空内容
             */
            void reset()
            {
                cur_ = data_;
            }

            /*
             * @brief   缓冲区内容清零
             */
            void bzero()
            {
                memset(data_, 0, sizeof(data_));
            }

            /*
             * @brief   将缓冲区的内容转化为 string
             * @return  缓冲区内容
             */
            std::string toString() const
            {
                return std::string(data_, length());
            }

            /*
             * @brief       转换为 string_view
             * @return      结果
             */
            std::string_view toStringView() const
            {
                return std::string_view(data_, length());
            }

            /*
             * @brief   GDB
             */
            const char* debugString();

            /*
             * @brief       设置 cookie 函数，用于程序异常崩溃调试
             * @param[in]   cookie 函数
             */
            void setCookie(cookieFunc cookie) {
                cookie_ = cookie;
            }

        private:
            /*
             * @brief   获取缓冲区的末尾
             * @return  缓冲区尾
             */
            const char* end() const
            {
                return data_ + sizeof(data_);
            }

            /*
             * @brief       cookie 函数，用于程序异常崩溃调试
             */
            static void cookieStart();
            static void cookieEnd();

        private:
            cookieFunc cookie_;  // cookie
            char* cur_;  // 缓冲区的当前指针, 指向有效字符的末尾
            char data_[SIZE];  // 缓冲区
        };
    }

    /*
     * @brief   日志的流操作，语法类似 <iostream>
     */
    class HALCYON_LOG_DLL_DECL LogStream : base::noncopyable
    {
    public:
        using Buffer = detail::FixedBuffer<detail::kSmallBuffer>;
        /*
         * @brief   重载 << 操作符，用于流操作
         */
         /*
          * @brief  bool 类型
          */
        LogStream& operator<<(bool b)
        {
            buffer_.append(b ? "1" : "0", 1);
            return *this;
        }

        /*
         * @brief   整数
         */
        LogStream& operator<<(short);
        LogStream& operator<<(unsigned short);
        LogStream& operator<<(int);
        LogStream& operator<<(unsigned int);
        LogStream& operator<<(long);
        LogStream& operator<<(unsigned long);
        LogStream& operator<<(long long);
        LogStream& operator<<(unsigned long long);

        /*
         * @brief   指针
         */
        LogStream& operator<<(const void*);

        /*
         * @brief   浮点数
         */
        LogStream& operator<<(double);
        LogStream& operator<<(float f)
        {
            *this << static_cast<double>(f);
            return *this;
        }

        /*
         * @brief   单个字符
         */
        LogStream& operator<<(char c)
        {
            buffer_.append(&c, 1);
            return *this;
        }

        /*
         * @brief   字符串
         */
        LogStream& operator<<(const char* str)
        {
            if (str == nullptr) {
                buffer_.append("(nullptr)", 9);
            }
            else {
                buffer_.append(str, strlen(str));
            }
            return *this;
        }
        LogStream& operator<<(const unsigned char* str)
        {
            return operator<<(reinterpret_cast<const char*>(str));
        }

        /*
         * @brief   std::string, std::string_view
         */
        LogStream& operator<<(const std::string& str)
        {
            buffer_.append(str.c_str(), str.length());
            return *this;
        }
        LogStream& operator<<(const std::string_view& sv)
        {
            buffer_.append(sv.data(), sv.size());
            return *this;
        }

        /*
         * @brief   FixedBuffer<smallBuffer>
         */
        LogStream& operator<<(const Buffer& buffer)
        {
            *this << buffer.toStringView();
            return *this;
        }

        /*
         * @brief   线程ID
         */
        LogStream& operator<<(const std::thread::id& threadId);

        /*
         * @brief       添加数据到缓冲区中
         * @param[in]   数据
         * @param[in]   数据长度
         */
        void append(const char* data, const size_t len)
        {
            buffer_.append(data, len);
        }

        /*
         * @brief   获取缓冲区对象
         * @return  缓冲区对象
         */
        const Buffer& buffer() const
        {
            return buffer_;
        }

        /*
         * @brief   重置缓冲区
         */
        void resetBuffer()
        {
            buffer_.reset();
        }

    private:
        /*
         * @brief   静态检查
         */
        void staticCheck();

        /*
         * @brief   整数的 format
         */
        template<typename T>
        void formatInteger(T);

    private:
        Buffer buffer_;  // 缓冲区
        static const int kMaxNumericSize = 32;
    };

    /*
     * @brief   格式化操作
     */
    class HALCYON_LOG_DLL_DECL Fmt
    {
    public:
        template<typename T>
        Fmt(const char* fmt, T val);

        /*
         * @brief   获取数据
         * @return  数据
         */
        const char* data() const
        {
            return buffer_;
        }

        /*
         * @brief   获取长度
         * @return  长度
         */
        size_t length() const
        {
            return length_;
        }

    private:
        char buffer_[32];  // 数据内容
        size_t length_;  // 数据长度
    };

    inline LogStream& operator<<(LogStream& s, const Fmt& fmt)
    {
        s.append(fmt.data(), fmt.length());
        return s;
    }

    // Format quantity n in SI units (k, M, G, T, P, E).
    // The returned string is atmost 5 characters long.
    // Requires n >= 0
    std::string formatSI(long long n);

    // Format quantity n in IEC (binary) units (Ki, Mi, Gi, Ti, Pi, Ei).
    // The returned string is atmost 6 characters long.
    // Requires n >= 0
    std::string formatIEC(long long n);
}