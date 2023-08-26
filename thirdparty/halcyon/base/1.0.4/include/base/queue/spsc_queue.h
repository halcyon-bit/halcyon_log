#ifndef BASE_SPSC_QUEUE_H
#define BASE_SPSC_QUEUE_H

#if defined(__s390__) || defined(__s390x__)
#define BASE_CACHELINE_BYTES    256
#elif defined(powerpc) || defined(__powerpc__) || defined(__ppc__)
#define BASE_CACHELINE_BYTES    128
#else
#define BASE_CACHELINE_BYTES    64
#endif

#include <base/common/noncopyable.h>

#include <atomic>
#include <memory>
#include <base/utility/type.h>

BASE_BEGIN_NAMESPACE

/// 单消费者、单生产者的无锁队列

namespace detail
{
    /// 循环队列基类
    template<typename T>
    class RingBuffer : noncopyable
    {
    protected:
        RingBuffer()
            : write_index_(0), read_index_(0)
        {}

        /**
         * @brief       入队
         * @param[in]   数据
         * @param[in]   队列头指针
         * @param[in]   队列总大小
         * @return      是否成功
         */
        bool push(const T& t, T* buffer, size_t max_size)
        {
            const size_t write_index = write_index_.load(std::memory_order_relaxed);
            const size_t next = nextIndex(write_index, max_size);
            
            if (read_index_.load(std::memory_order_acquire) == next)
                return false;  // 队列已满

            // 入队，调整位置
            new (buffer + write_index) T(t);  // 拷贝构造
            write_index_.store(next, std::memory_order_release);
            return true;
        }
        bool push(T&& t, T* buffer, size_t max_size)
        {
            const size_t write_index = write_index_.load(std::memory_order_relaxed);
            const size_t next = nextIndex(write_index, max_size);

            if (read_index_.load(std::memory_order_acquire) == next)
                return false;  // 队列已满

            // 入队，调整位置
            new (buffer + write_index) T(std::forward<T>(t));  // 移动构造
            write_index_.store(next, std::memory_order_release);
            return true;
        }

        /**
         * @brief       出队
         * @param[out]  数据
         * @param[in]   队列头指针
         * @param[in]   队列总大小
         * @return      是否成功
         */
        bool take(T& value, T* buffer, size_t max_size)
        {
            const size_t write_index = write_index_.load(std::memory_order_acquire);
            const size_t read_index = read_index_.load(std::memory_order_relaxed);

            if (empty(write_index, read_index))
                return false;  // 队列为空

            T& t = buffer[read_index];
            value = std::move(t);
            t.~T();

            size_t next = nextIndex(read_index, max_size);
            read_index_.store(next, std::memory_order_release);
            return true;
        }

        /**
         * @brief       判断队列是否为空
         * @return      是否为空
         */
        bool empty() const
        {
            return empty(write_index_.load(std::memory_order_relaxed), read_index_.load(std::memory_order_relaxed));
        }

        /**
         * @brief       清空队列
         * @return      清除的数量
         */
        size_t clear(T* buffer, size_t max_size)
        {
            const size_t write_index = write_index_.load(std::memory_order_acquire);
            const size_t read_index = read_index_.load(std::memory_order_relaxed);

            // 当前队列中元素数量
            const size_t count = readAvailable(write_index, read_index, max_size);

            if (count == 0)
                return 0;

            size_t new_read_index = read_index + count;
            if (new_read_index > max_size) {
                const size_t end_pos = count - (max_size - read_index);

                destory(buffer + read_index, buffer + max_size);
                destory(buffer, buffer + end_pos);

                new_read_index -= max_size;
            } else {
                destory(buffer + read_index, buffer + new_read_index);

                if (new_read_index == max_size)
                    new_read_index = 0;
            }
            read_index_.store(new_read_index, std::memory_order_release);
            return count;
        }

    private:
        /**
         * @brief       获取队列中下一个可用位置
         * @param[in]   当前位置
         * @param[in]   队列总大小
         * @return      下一个位置
         */
        size_t nextIndex(size_t index, size_t max_size) const
        {
            size_t ret = index + 1;
            while (ret >= max_size)
                ret -= max_size;
            return ret;
        }

        /**
         * @brief       获取当前队列中元素数量
         * @param[in]   写位置
         * @param[in]   读位置
         * @param[in]   队列总大小
         * @return      元素数量
         */
        size_t readAvailable(size_t write_index, size_t read_index, size_t max_size) const
        {
            if (write_index >= read_index)
                return write_index - read_index;

            return write_index + max_size - read_index;
        }

        /**
         * @brief       获取当前队列空闲数量
         * @param[in]   写位置
         * @param[in]   读位置
         * @param[in]   队列总大小
         * @return      空闲数量
         */
        size_t writeAvailable(size_t write_index, size_t read_index, size_t max_size) const
        {
            size_t ret = read_index - write_index - 1;
            if (write_index >= read_index)
                ret += max_size;
            return ret;
        }

        /**
         * @brief       判断队列是否为空
         * @param[in]   写位置
         * @param[in]   读位置
         * @return      是否为空
         */
        bool empty(size_t write_index, size_t read_index) const
        {
            return write_index == read_index;
        }

        void destory(T* first, T* last)
        {
            for (; first != last; ++first) {
                first->~T();
            }
        }

    protected:
        static constexpr int kPaddingSize = BASE_CACHELINE_BYTES - sizeof(size_t);

        //! 写地址
        std::atomic<size_t> write_index_;
        //! 将 write_index_、read_index_ 分离在两个 cache_line 上，避免不同线程之间的缓存竞争。
        char padding[kPaddingSize]{ 0 };
        //! 读地址
        std::atomic<size_t> read_index_;
    };

    /// 编译期循环队列
    template<typename T, std::size_t MaxSize>
    class CompileRingBuffer : public RingBuffer<T>
    {
        using size_type = std::size_t;

    public:
        ~CompileRingBuffer()
        {
            RingBuffer<T>::clear(data(), kMaxSize);
        }

    public:
        /**
         * @brief       入队
         * @param[in]   数据
         * @return      是否成功
         */
        bool push(const T& t)
        {
            return RingBuffer<T>::push(t, data(), kMaxSize);
        }
        bool push(T&& t)
        {
            return RingBuffer<T>::push(std::forward<T>(t), data(), kMaxSize);
        }

        /**
         * @brief       出队
         * @param[out]  数据
         * @return      是否成功
         */
        bool take(T& t)
        {
            return RingBuffer<T>::take(t, data(), kMaxSize);
        }

        /**
         * @brief       判断队列是否为空
         * @return      是否为空
         */
        bool empty() const
        {
            return RingBuffer<T>::empty();
        }

    private:
        /**
         * @brief       获取队列地址
         * @return      地址
         */
        T* data()
        {
            return reinterpret_cast<T*>(&storage_);
        }
        const T* data() const
        {
            return reinterpret_cast<const T*>(&storage_);
        }

    private:
        static constexpr size_type kMaxSize = MaxSize + 1;

        using storage_type = std::aligned_storage_t<kMaxSize * sizeof(T), std::alignment_of<T>::value>;
        //! 循环队列
        storage_type storage_;
    };

    /// 运行期循环队列
    template<typename T, typename Alloc = std::allocator<T>>
    class RunTimeRingBuffer : public RingBuffer<T>, private Alloc
    {
        using size_type = std::size_t;
        using allocator_traits = std::allocator_traits<Alloc>;
        using pointer = typename allocator_traits::pointer;

    public:
        /**
         * @brief       构造函数
         * @param[in]   队列大小
         */
        explicit RunTimeRingBuffer(size_type max_size)
        {
            max_size_ = max_size + 1;
            Alloc& alloc = *this;
            array_ = allocator_traits::allocate(alloc, max_size_);
        }

        ~RunTimeRingBuffer()
        {
            RingBuffer<T>::clear(&*array_, max_size_);

            Alloc& alloc = *this;
            allocator_traits::deallocate(alloc, array_, max_size_);
        }

    public:
        /**
         * @brief       入队
         * @param[in]   数据
         * @return      是否成功
         */
        bool push(const T& t)
        {
            return RingBuffer<T>::push(t, &*array_, max_size_);
        }
        bool push(T&& t)
        {
            return RingBuffer<T>::push(std::forward<T>(t), &*array_, max_size_);
        }

        /**
         * @brief       出队
         * @param[out]  数据
         * @return      是否成功
         */
        bool take(T& t)
        {
            return RingBuffer<T>::take(t, &*array_, max_size_);
        }

        /**
         * @brief       判断队列是否为空
         * @return      是否为空
         */
        bool empty() const
        {
            return RingBuffer<T>::empty();
        }

    private:
        //! 队列大小
        size_type max_size_;
        //! 循环队列
        pointer array_;
    };
}

/**
 * @brief   线程安全的无锁队列（适用于单生产者、单消费者）
 * @ps      MaxSize 为 0，队列动态创建，运行期队列；不为 0，队列为静态数组，编译期队列
 */
template<typename T, std::size_t MaxSize = 0>
class SPSCQueue 
    : private std::conditional<MaxSize == 0, detail::RunTimeRingBuffer<T>, detail::CompileRingBuffer<T, MaxSize>>::type
{
public:
    using size_type = std::size_t;
    using ringbuffer = std::conditional_t<MaxSize == 0, detail::RunTimeRingBuffer<T>, detail::CompileRingBuffer<T, MaxSize>>;
    static constexpr bool kCompileBuffer = MaxSize != 0;

public:
    /**
     * @brief   构造函数
     * @ps      编译期队列
     */
    SPSCQueue()
    {
        static_assert(kCompileBuffer, "template arg MaxSize should not be zero");
    }

    /**
     * @brief       构造函数
     * @param[in]   队列大小
     * @ps          运行期队列
     */
    explicit SPSCQueue(size_type max_size)
        : ringbuffer(max_size)
    {
        static_assert(!kCompileBuffer, "template arg MaxSize should be zero");
    }

    /**
     * @brief       入队
     * @param[in]   数据
     * @return      是否成功
     */
    bool push(const T& t)
    {
        return ringbuffer::push(t);
    }
    bool push(T&& t)
    {
        return ringbuffer::push(std::forward<T>(t));
    }

    /**
     * @brief       出队
     * @param[out]  数据
     * @return      是否成功
     */
    bool take(T& t)
    {
        return ringbuffer::take(t);
    }

    /**
     * @brief       判断队列是否为空
     * @return      是否为空
     */
    bool empty() const
    {
        return ringbuffer::empty();
    }
};

BASE_END_NAMESPACE

#endif