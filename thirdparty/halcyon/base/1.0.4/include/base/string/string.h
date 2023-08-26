#pragma once

#include <base/utility/utility.h>

#include <atomic>
#include <cstring>
#include <cassert>
#include <algorithm>

#ifdef USE_HALCYON_STRING_VIEW
#include <base/string/string_view.h>
#else
#include <string_view>
#endif

BASE_BEGIN_NAMESPACE

//! 是否不启用SSO（短字符串的存储方式）
static constexpr bool kDisableSSO{ false };

namespace detail
{
    /// 内存相关函数(方便替换成内存池)
    /**
     * @brief   申请内存(失败抛异常)
     */
    inline void* checkMalloc(size_t size)
    {
        void* p = malloc(size);
        if (nullptr == p) {
            throw std::bad_alloc();
        }
        return p;
    }

    /**
     * @brief   重新申请内存(失败抛异常)
     */
    inline void* checkRealloc(void* p, size_t size)
    {
        void* ptr = realloc(p, size);
        if (nullptr == ptr) {
            throw std::bad_alloc();
        }
        return ptr;
    }

    /**
     * @brief       重新申请内存
     * @param[in]   旧的内存地址
     * @param[in]   已使用的大小
     * @param[in]   旧内存容量
     * @param[in]   新内存容量
     * @ps          如果 (ocapacity - osize) * 2 > osize，即 osize < (2/3 * ocapacity)，
     *            说明当前分配的内存利用率较低，此时如果使用 realloc，成本会高于直接 
     *            malloc(ncapacity) + memcpy + free(ocapacity)；否则直接 realloc
     */
    inline void* smartRealloc(void* p, size_t osize, size_t ocapacity, size_t ncapacity)
    {
        assert(p != nullptr);
        assert(osize <= ocapacity && ocapacity < ncapacity);
        size_t slack = ocapacity - osize;
        if (slack * 2 > osize) {
            void* result = checkMalloc(ncapacity);
            memcpy(result, p, osize);
            free(p);
            return result;
        } else {
            return checkRealloc(p, ncapacity);
        }
    }

    template<typename InIt, typename OutIt>
    inline std::pair<InIt, OutIt> copy_n(InIt b, typename std::iterator_traits<InIt>::difference_type n, OutIt d)
    {
        for (; n != 0; --n, ++b, ++d) {
            *d = *b;
        }
        return std::make_pair(b, d);
    }
}

/**
 * @brief   字符串的核心部分，字符串分为短、中、长三类
 *  短字符串使用 union 中的 CharT small_ 存储字符串，即对象本身的栈空间 (<= 23 chars)。
 *  中字符串使用 union 中的 MediumLarge ml_ [24 ~ 288] chars
 *      CharT* data_：指向分配在堆上的字符串
 *      size_t size_：字符串长度
 *      size_t capacity_：字符串容量
 *  长字符串使用 MediumLarge ml_ 和 RefCounted (> 288 chars)
 *      RefCounted.refCount_：共享字符串的引用计数。
 *      RefCounted.data_[1]：存放字符串。
 *      ml_.data_ 指向 RefCounted.data_，ml_.size_ 与 ml_.capacity_ 的含义不变。
 *  字符串类型存储在 union 中 bytes_ 的最后一个字节
 */
template<typename CharT, typename Traits = std::char_traits<CharT>>
class string_core
{
public:  // 构造函数
    string_core() noexcept
    {
        reset();
    }

    /**
     * @brief       构造函数
     * @param[in]   字符串数据
     * @param[in]   字符串长度
     * @param[in]   是否不启用SSO（短字符串的存储方式）
     */
    string_core(const CharT* const data, size_t size, bool disableSSO = kDisableSSO)
    {
        if (!disableSSO && size <= kMaxSmallSize) {
            // 短字符串
            initSmall(data, size);
        } else if (size <= kMaxMediumSize) {
            // 中字符串
            initMedium(data, size);
        } else {
            // 长字符串
            initLarge(data, size);
        }
        assert(this->size() == size);
        assert(size == 0 || memcmp(this->data(), data, size * sizeof(CharT)) == 0);
    }

    string_core(const string_core& rhs)
    {
        assert(&rhs != this);
        switch (rhs.type())
        {
        case Type::emTypeSmall:
            copySmall(rhs);
            break;
        case Type::emTypeMedium:
            copyMedium(rhs);
            break;
        case Type::emTypeLarge:
            copyLarge(rhs);
            break;
        default:
            assert(0);
        }
        assert(size() == rhs.size());
        assert(memcmp(data(), rhs.data(), size() * sizeof(CharT)) == 0);
    }
    string_core(string_core&& rhs) noexcept
    {
        ml_ = rhs.ml_;
        rhs.reset();
    }

    // 禁止赋值
    string_core& operator=(const string_core& rhs) = delete;

    ~string_core() noexcept
    {
        if (type() == Type::emTypeSmall) {
            return;
        }
        destroyMediumLarge();
    }

public:  // 访问
    const CharT* data() const
    {
        return c_str();
    }
    CharT* data()
    {
        return c_str();
    }

    /**
     * @brief   获取可改变的数据，主要是针对长字符串
     *          写时复制(COW)
     */
    CharT* mutableData()
    {
        switch (type())
        {
        case Type::emTypeSmall:
            return small_;
        case Type::emTypeMedium:
            return ml_.data_;
        case Type::emTypeLarge:
            return mutableDataLarge();
        default:
            assert(0);
            return nullptr;
        }
    }

    const CharT* c_str() const
    {
        const CharT* ptr = ml_.data_;
        ptr = (type() == Type::emTypeSmall) ? small_ : ptr;
        return ptr;
    }

public:  // 容量
    size_t size() const
    {
        using UCharT = typename std::make_unsigned<CharT>::type;
        // 非短字符串，获取的 maybeSmallSize 是个非常大的值(负值)
        size_t maybe_small_size = size_t(kMaxSmallSize) - size_t(static_cast<UCharT>(small_[kLastChar]));
        return (static_cast<int>(maybe_small_size) >= 0) ? maybe_small_size : ml_.size_;
    }

    size_t capacity() const
    {
        switch (type())
        {
        case Type::emTypeSmall:
            return kMaxSmallSize;
        case Type::emTypeLarge:
            // 当字符串引用大于 1 时，直接返回 size。因为此时
            // 的 capacity 是没有意义的，任何 append data 操作
            // 都会触发一次 cow
            if (RefCounted::getRefCount(ml_.data_) > 1) {
                return ml_.size_;
            }
            break;
        case Type::emTypeMedium:
        default:
            break;
        }
        return ml_.getCapacity();
    }

public:  // 修改
    void push_back(CharT c)
    {
        *expandNoinit(1, true) = c;
    }

    /**
     * @brief       收缩字符串长度
     * @param[in]   缩小量 Δ
     * @ps          缩小量不能超过字符串当前长度
     */
    void shrink(size_t delta)
    {
        if (type() == Type::emTypeSmall) {
            shrinkSmall(delta);
        } else if (type() == Type::emTypeMedium || RefCounted::getRefCount(ml_.data_) == 1) {
            // 中字符串，或者尚未被共享的长字符串
            shrinkMedium(delta);
        } else {
            // 被共享的长字符串
            shrinkLarge(delta);
        }
    }

    /**
     * @brief       调整字符串的容量(扩张)
     * @param[in]   新容量
     * @param[in]   是否不启用SSO（短字符串的存储方式）
     */
    void reserve(size_t new_capacity, bool disableSSO = kDisableSSO)
    {
        switch (type()) 
        {
        case Type::emTypeSmall:
            reserveSmall(new_capacity, disableSSO);
            break;
        case Type::emTypeMedium:
            reserveMedium(new_capacity);
            break;
        case Type::emTypeLarge:
            reserveLarge(new_capacity);
            break;
        default:
            assert(0);
        }
        assert(capacity() >= new_capacity);
    }

    /**
     * @brief       扩充字符串
     * @param[in]   扩容量 Δ
     * @param[in]   容量是否倍率增长(1.5倍)
     * @param[in]   是否不启用SSO（短字符串的存储方式）
     * @return      扩容后，空闲字符的起始位置
     * @ps          扩充后，会在末尾加上 '\0'
     */
    CharT* expandNoinit(size_t delta, bool exp_growth = false, bool disableSSO = kDisableSSO);

public:
    void swap(string_core& rhs)
    {
        const auto tmp = ml_;
        ml_ = rhs.ml_;
        rhs.ml_ = tmp;
    }

    /**
     * @brief   字符串是否被共享(长字符串)
     */
    bool isShared() const
    {
        return type() == Type::emTypeLarge && RefCounted::getRefCount(ml_.data_) > 1;
    }

private:
    /**
     * @brief   获取字符串
     */
    CharT* c_str()
    {
        CharT* ptr = ml_.data_;
        ptr = (type() == Type::emTypeSmall) ? small_ : ptr;
        return ptr;
    }

    /**
     * @brief   用于初始化
     */
    void reset()
    {
        setSmallSize(0);
    }

    /**
     * @brief   清理内存或减少引用(析构)
     */
    void destroyMediumLarge() noexcept
    {
        const Type t = type();
        assert(t != Type::emTypeSmall);
        if (t == Type::emTypeMedium) {
            free(ml_.data_);
        } else {
            // 减少计数，为 0 自动释放
            RefCounted::decRefCount(ml_.data_);
        }
    }

private:
    /**
     * @brief       初始化字符串
     * @param[in]   字符串数据
     * @param[in]   字符串长度
     */
    void initSmall(const CharT* data, size_t size);
    void initMedium(const CharT* data, size_t size);
    void initLarge(const CharT* data, size_t size);

private:
    /**
     * @brief       拷贝字符串
     */
    void copySmall(const string_core& rhs);
    void copyMedium(const string_core& rhs);
    void copyLarge(const string_core& rhs);

private:
    /**
     * @brief       扩张字符串
     * @param[in]   字符串容量
     */
    void reserveSmall(size_t new_capacity, bool disableSSO);
    void reserveMedium(size_t new_capacity);
    void reserveLarge(size_t new_capacity);
    
private:
    /**
     * @brief       收缩字符串
     * @param[in]   缩小量 Δ
     */
    void shrinkSmall(size_t delta);
    void shrinkMedium(size_t delta);
    void shrinkLarge(size_t delta);

private:
    void unshare(size_t new_capacity = 0);
    CharT* mutableDataLarge();

private:
    /**
     * @brief   获取短字符串的长度
     */
    size_t getSmallSize() const
    {
        assert(type() == Type::emTypeSmall);
        size_t size = static_cast<size_t>(small_[kLastChar]);
        assert(size <= kMaxSmallSize);
        return kMaxSmallSize - size;
    }

    /**
     * @brief   设置短字符串的长度
     */
    void setSmallSize(size_t size)
    {
        // small string 存放的 size 不是真正的 size，是 kMaxSmallSize - size，
        // 原因是这样 small string 可以多存储一个字节。因为假如存储 size 的话，
        // small_ 中最后两个字节就得是 \0 和 size，但是存储 kMaxSmallSize - size，
        // 当 size == kMaxSmallSize 时，small_ 的最后一个字节恰好也是 \0。
        assert(size <= kMaxSmallSize);
        small_[kLastChar] = char(kMaxSmallSize - size);
        small_[size] = '\0';
        assert(type() == Type::emTypeSmall && this->size() == size);
    }

private:  // 数据结构
    /// 长字符串（引用计数）
    struct RefCounted
    {
        //! 共享字符串的引用计数
        std::atomic<size_t> count_;
        //! 柔性数组，存放字符串
        CharT data_[1];

        /**
         * @brief   获取 data_ 的偏移(基于 RefCounted)
         */
        constexpr static size_t getDataOffset()
        {
            return offsetof(RefCounted, data_);
        }

        /**
         * @brief   通过 data_ 获取 RefCounted 结构体
         */
        static RefCounted* fromData(CharT* data)
        {
            return static_cast<RefCounted*>(static_cast<void*>(static_cast<unsigned char*>(
                static_cast<void*>(data)) - getDataOffset()));
        }

        /**
         * @brief   获取引用计数
         */
        static size_t getRefCount(CharT* data)
        {
            return fromData(data)->count_.load(std::memory_order_acquire);
        }

        /**
         * @brief   增加引用计数
         */
        static void incRefCount(CharT* data)
        {
            fromData(data)->count_.fetch_add(1, std::memory_order_acq_rel);
        }

        /**
         * @brief   减少引用计数
         */
        static void decRefCount(CharT* data)
        {
            const auto ref = fromData(data);
            size_t old_count = ref->count_.fetch_sub(1, std::memory_order_acq_rel);
            assert(old_count > 0);
            if (old_count == 1) {
                // 引用计数为 0，释放内存
                free(ref);
            }
        }

        /**
         * @brief       创建一个 RefCounted
         * @param[in]   内存大小
         */
        static RefCounted* allocate(size_t size)
        {
            // 申请内存的总大小
            const size_t alloc_size = getDataOffset() + (size + 1) * sizeof(CharT);
            RefCounted* result = static_cast<RefCounted*>(detail::checkMalloc(alloc_size));
            result->count_.store(1, std::memory_order_release);
            return result;
        }

        /**
         * @brief       创建一个 RefCounted
         * @param[in]   字符串
         * @param[in]   字符串长度
         */
        static RefCounted* allocate(const CharT* data, size_t size)
        {
            RefCounted* result = allocate(size);
            if (size > 0) {
                Traits::copy(result->data_, data, size);
            }
            return result;
        }

        /**
         * @brief       重新分配内存
         * @param[in]   原内存地址
         * @param[in]   原字符串长度
         * @param[in]   原字符串容量
         * @param[in]   新字符串容量
         */
        static RefCounted* reallocate(CharT* const data, size_t osize, size_t ocapacity, size_t ncapacity)
        {
            assert(ncapacity > 0 && ncapacity > osize);
            constexpr size_t offset = getDataOffset();
            // 新容量大小
            const size_t alloc_size = offset + (ncapacity + 1) * sizeof(CharT);
            const auto ref = fromData(data);  // 旧的内存
            // 当前没有被共享
            assert(ref->count_.load(std::memory_order_acquire) == 1);
            // 重新分配内存
            auto result = static_cast<RefCounted*>(detail::smartRealloc(ref, 
                offset + (osize + 1) * sizeof(CharT),
                offset + (ocapacity + 1) * sizeof(CharT),
                alloc_size));
            assert(result->count_.load(std::memory_order_acquire) == 1);
            return result;
        }
    };
    
private:  // 数据
    /**
     * @brief   字符串类型（用一字节的高两位保存）（小端序）
     * @ps      短字符串存储在 small_ 的最后一个字节
     *          中、长字符串存储在 MediumLarge.capacity_ 的最后一个字节
     *          可以用 bytes_ 定位，sizeof(MediumLarge) == sizeof(small_)
     */
    enum class Type : uint8_t
    {
        emTypeSmall = 0,  // 短字符串
        emTypeMedium = 0x40,  // 中字符串
        emTypeLarge = 0x80,  // 长字符串
    };

    /**
     * @brief   获取字符串的类型
     * @return  类型
     */
    constexpr Type type() const
    {
        return static_cast<Type>(bytes_[kLastChar] & kTypeExtractMask);
    }

private:
    /// 中、长字符串结构体
    struct MediumLarge
    {
        //! 中：指向分配在堆上的字符串，长：指向 RefCounted.data_ 成员
        CharT* data_;
        //! 字符串长度
        size_t size_;
        //! 字符串容量(申请的内存大小)，也包含字符串类型
        size_t capacity_;

        /**
         * @brief   获取字符串容量
         */
        constexpr size_t getCapacity() const
        {
            return capacity_ & kCapacityExtractMask;
        }

        /**
         * @brief       设置字符串容量与字符串类型
         * @param[in]   字符串容量
         * @param[in]   字符串类型
         */
        void setCapacity(size_t cap, Type type)
        {
            capacity_ = cap | (static_cast<size_t>(type) << kTypeShift);
        }
    };

    union {
        //! 方便获取 MediumLarge 的最后一个字节，即字符串的类型
        uint8_t bytes_[sizeof(MediumLarge)];
        //! 短字符串，存贮在栈上，字符串长度存储在最后一个字节中(也包含字符串类型)
        CharT small_[sizeof(MediumLarge) / sizeof(CharT)];
        //! 中、长字符串，在堆上
        MediumLarge ml_;
    };

    /// 一些常量
    //! 用于获取结构体最后一个字节的位置，用于获取字符串类型
    constexpr static size_t kLastChar = sizeof(MediumLarge) - 1;
    //! 短字符串的最大长度
    constexpr static size_t kMaxSmallSize = kLastChar / sizeof(CharT);
    //! 中字符串的最大长度
    constexpr static size_t kMaxMediumSize = 288 / sizeof(CharT);
    //! 字符串类型的标记掩码
    constexpr static uint8_t kTypeExtractMask = 0xC0;
    //! 字符串类型的位置偏移
    constexpr static size_t kTypeShift = (sizeof(size_t) - 1) * 8;
    //! 字符串容量的标记掩码
    constexpr static size_t kCapacityExtractMask = ~(size_t(kTypeExtractMask) << kTypeShift);
};

template<typename CharT, typename Traits>
CharT* string_core<CharT, Traits>::expandNoinit(size_t delta, bool exp_growth, bool disableSSO)
{
    assert(capacity() >= size());
    size_t len, new_len;
    if (type() == Type::emTypeSmall) {
        len = getSmallSize();
        new_len = len + delta;
        if (!disableSSO && new_len <= kMaxSmallSize) {
            // 短字符串，并且新长度仍然满足
            setSmallSize(new_len);
            return small_ + len;
        }
        // 不满足，扩展为其他类型字符串
        reserveSmall(exp_growth ? std::max(new_len, 2 * kMaxSmallSize) : new_len, disableSSO);
    } else {
        len = ml_.size_;
        new_len = len + delta;
        if (new_len > capacity()) {
            // 新的容量 > 原先容量，扩容
            reserve(exp_growth ? std::max(new_len, 1 + capacity() * 3 / 2) : new_len);
        }
    }
    assert(capacity() >= new_len);
    assert(type() == Type::emTypeMedium || type() == Type::emTypeLarge);
    ml_.size_ = new_len;
    ml_.data_[new_len] = '\0';
    assert(size() == new_len);
    return ml_.data_ + len;
}

template<typename CharT, typename Traits>
inline void string_core<CharT, Traits>::initSmall(const CharT* data, size_t size)
{
    // 判断传入的字符串地址是否为内存对齐的，可以提高效率
    if ((reinterpret_cast<size_t>(data) & (sizeof(size_t) - 1)) == 0) {
        const size_t byte_size = size * sizeof(CharT);  // 数据长度
        constexpr size_t word_width = sizeof(size_t);  // 字长

        // 短字符串，最长为 23 bytes(x64)
        switch ((byte_size + word_width - 1) / word_width)
        {
        case 3:
            ml_.capacity_ = reinterpret_cast<const size_t*>(data)[2];
            FALLTHROUGH;  // 忽略编译器的警告，没有 break，C++17
        case 2:
            ml_.size_ = reinterpret_cast<const size_t*>(data)[1];
            FALLTHROUGH;
        case 1:
            ml_.data_ = *reinterpret_cast<CharT**>(const_cast<CharT*>(data));
            FALLTHROUGH;
        case 0:
            break;
        }
    } else {
        if (size != 0) {
            Traits::copy(small_, data, size);
        }
    }
    setSmallSize(size);
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::initMedium(const CharT* data, size_t size)
{
    const size_t alloc_size = (size + 1) * sizeof(CharT);
    ml_.data_ = static_cast<CharT*>(detail::checkMalloc(alloc_size));
    if (size > 0) {
        Traits::copy(ml_.data_, data, size);
    }
    ml_.size_ = size;
    ml_.setCapacity(size, Type::emTypeMedium);
    ml_.data_[size] = '\0';
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::initLarge(const CharT* data, size_t size)
{
    const auto newRc = RefCounted::allocate(data, size);
    ml_.data_ = newRc->data_;
    ml_.size_ = size;
    ml_.setCapacity(size, Type::emTypeLarge);
    ml_.data_[size] = '\0';
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::copySmall(const string_core& rhs)
{
    ml_ = rhs.ml_;
    assert(type() == Type::emTypeSmall);
    assert(size() == rhs.size());
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::copyMedium(const string_core& rhs)
{
    const size_t alloc_size = (rhs.ml_.size_ + 1) * sizeof(CharT);
    ml_.data_ = static_cast<CharT*>(detail::checkMalloc(alloc_size));
    Traits::copy(ml_.data_, rhs.ml_.data_, alloc_size);
    ml_.size_ = rhs.ml_.size_;
    ml_.setCapacity(alloc_size / sizeof(CharT) - 1, Type::emTypeMedium);
    assert(type() == Type::emTypeMedium);
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::copyLarge(const string_core& rhs)
{
    ml_ = rhs.ml_;
    // 增加引用计数
    RefCounted::incRefCount(ml_.data_);
    assert(type() == Type::emTypeLarge);
    assert(size() == rhs.size());
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::reserveSmall(size_t new_capacity, bool disableSSO)
{
    assert(type() == Type::emTypeSmall);
    if (!disableSSO && new_capacity <= kMaxSmallSize) {
        // 新容量仍然满足最长短字符串，则什么都不做
        return;
    } else if (new_capacity <= kMaxMediumSize) {
        // 超过最长短字符串，但满足中字符串
        const size_t alloc_size = (new_capacity + 1) * sizeof(CharT);
        CharT* const newData = static_cast<CharT*>(detail::checkMalloc(alloc_size));
        const size_t size = getSmallSize();

        Traits::copy(newData, small_, size + 1);
        ml_.data_ = newData;
        ml_.size_ = size;
        ml_.setCapacity(new_capacity, Type::emTypeMedium);
    } else {
        // 变为长字符串
        const auto newRc = RefCounted::allocate(new_capacity);
        const size_t size = getSmallSize();

        Traits::copy(newRc->data_, small_, size + 1);
        ml_.data_ = newRc->data_;
        ml_.size_ = size;
        ml_.setCapacity(new_capacity, Type::emTypeLarge);
        assert(capacity() >= new_capacity);
    }
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::reserveMedium(size_t new_capacity)
{
    assert(type() == Type::emTypeMedium);
    if (new_capacity <= ml_.getCapacity()) {
        // 原先容量 > 新容量，则无需调整
        return;
    }

    if (new_capacity <= kMaxMediumSize) {
        // 仍然为中字符串，新的容量
        size_t alloc_size = (new_capacity + 1) * sizeof(CharT);
        ml_.data_ = static_cast<CharT*>(detail::smartRealloc(ml_.data_,
            (ml_.size_ + 1) * sizeof(CharT),
            (ml_.getCapacity() + 1) * sizeof(CharT),
            alloc_size));
        ml_.setCapacity(new_capacity, Type::emTypeMedium);
    } else {
        // 变为长字符串
        string_core tmp;
        tmp.reserve(new_capacity);
        tmp.ml_.size_ = ml_.size_;
        Traits::copy(tmp.ml_.data_, ml_.data_, ml_.size_ + 1);
        tmp.swap(*this);
        assert(capacity() >= new_capacity);
    }
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::reserveLarge(size_t new_capacity)
{
    assert(type() == Type::emTypeLarge);
    if (RefCounted::getRefCount(ml_.data_) > 1) {
        // 如果字符串处于共享状态，则创建一份新的
        unshare(new_capacity);
    } else {
        if (new_capacity > ml_.getCapacity()) {
            // 新容量更大，则尝试重新分配内存
            const auto newRc = RefCounted::reallocate(ml_.data_, ml_.size_,
                ml_.getCapacity(), new_capacity);
            ml_.data_ = newRc->data_;
            ml_.setCapacity(new_capacity, Type::emTypeLarge);
        }
        assert(capacity() >= new_capacity);
    }
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::shrinkSmall(size_t delta)
{
    assert(delta <= getSmallSize());
    setSmallSize(getSmallSize() - delta);
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::shrinkMedium(size_t delta)
{
    assert(ml_.size_ >= delta);
    ml_.size_ -= delta;
    ml_.data_[ml_.size_] = '\0';
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::shrinkLarge(size_t delta)
{
    assert(ml_.size_ >= delta);
    // 长字符串保持唯一，字符串与原先不同
    if (delta > 0) {
        string_core(ml_.data_, ml_.size_ - delta).swap(*this);
    }
}

template<typename CharT, typename Traits>
void string_core<CharT, Traits>::unshare(size_t new_capacity)
{
    assert(type() == Type::emTypeLarge);
    // 新字符串容量
    size_t capacity = std::max(new_capacity, ml_.getCapacity());
    const auto newRc = RefCounted::allocate(capacity);
    assert(capacity >= ml_.getCapacity());
    // copy data
    Traits::copy(newRc->data_, ml_.data_, ml_.size_ + 1);
    // 原先字符串的引用计数减1
    RefCounted::decRefCount(ml_.data_);
    ml_.data_ = newRc->data_;
    ml_.setCapacity(capacity, Type::emTypeLarge);
}

template<typename CharT, typename Traits>
CharT* string_core<CharT, Traits>::mutableDataLarge()
{
    assert(type() == Type::emTypeLarge);
    if (RefCounted::getRefCount(ml_.data_) > 1) {
        unshare();
    }
    return ml_.data_;
}


/// 字符串的迭代器，直接使用 char*，会有部分函数重载，调用异常
/// 0 可以隐式转换为 null
template<typename String>
class string_const_iterator
{
public:
    using iterator_category = std::random_access_iterator_tag;

    using value_type = typename String::value_type;
    using difference_type = typename String::difference_type;
    using reference = typename String::const_reference;
    using pointer = typename String::const_pointer;

public:
    string_const_iterator() noexcept
    {}

    explicit string_const_iterator(pointer ptr) noexcept
        : ptr_(ptr)
    {}

public:
    reference operator*() const noexcept
    {
        return *ptr_;
    }
    pointer operator->() const noexcept
    {
        return std::addressof(**this);
    }

    string_const_iterator& operator++() noexcept
    {
        ++ptr_;
        return *this;
    }
    string_const_iterator operator++(int) noexcept
    {
        string_const_iterator tmp = *this;
        ++* this;
        return tmp;
    }

    string_const_iterator& operator--() noexcept
    {
        --ptr_;
        return *this;
    }
    string_const_iterator operator--(int) noexcept
    {
        string_const_iterator tmp = *this;
        --* this;
        return tmp;
    }

    string_const_iterator& operator+=(difference_type off)
    {
        ptr_ += off;
        return *this;
    }
    string_const_iterator& operator-=(difference_type off)
    {
        return *this += -off;
    }

    string_const_iterator operator+(difference_type off) const noexcept
    {
        string_const_iterator tmp = *this;
        tmp += off;
        return tmp;
    }
    string_const_iterator operator-(difference_type off) const noexcept
    {
        string_const_iterator tmp = *this;
        tmp -= off;
        return tmp;
    }

    friend string_const_iterator operator+(difference_type off, string_const_iterator next) noexcept
    {
        next += off;
        return next;
    }

    difference_type operator-(const string_const_iterator& rhs) const noexcept
    {
        return ptr_ - rhs.ptr_;
    }

    reference operator[](difference_type off) const noexcept
    {
        return *(*this + off);
    }

    bool operator==(const string_const_iterator& rhs) const noexcept
    {
        return ptr_ == rhs.ptr_;
    }
    bool operator!=(const string_const_iterator& rhs) const noexcept
    {
        return !(*this == rhs);
    }
    bool operator<(const string_const_iterator& rhs) const noexcept
    {
        return ptr_ < rhs.ptr_;
    }
    bool operator>(const string_const_iterator& rhs) const noexcept
    {
        return rhs < *this;
    }
    bool operator<=(const string_const_iterator& rhs) const noexcept
    {
        return !(rhs < *this);
    }
    bool operator>=(const string_const_iterator& rhs) const noexcept
    {
        return !(*this < rhs);
    }

public:
    pointer ptr_{};
};

template<typename String>
class string_iterator : public string_const_iterator<String>
{
public:
    using iterator_category = std::random_access_iterator_tag;

    using value_type = typename String::value_type;
    using difference_type = typename String::difference_type;
    using reference = typename String::reference;
    using pointer = typename String::pointer;

    using base = string_const_iterator<String>;

    using base::base;

public:
    reference operator*() const noexcept
    {
        return const_cast<reference>(base::operator*());
    }
    pointer operator->() const noexcept
    {
        return std::addressof(**this);
    }

    string_iterator& operator++() noexcept
    {
        base::operator++();
        return *this;
    }
    string_iterator operator++(int) noexcept
    {
        string_iterator tmp = *this;
        base::operator++();
        return tmp;
    }

    string_iterator& operator--() noexcept
    {
        base::operator--();
        return *this;
    }
    string_iterator operator--(int) noexcept
    {
        string_iterator tmp = *this;
        base::operator--();
        return tmp;
    }

    string_iterator& operator+=(difference_type off) noexcept
    {
        base::operator+=(off);
        return *this;
    }
    string_iterator& operator-=(difference_type off) noexcept
    {
        base::operator-=(off);
        return *this;
    }

    string_iterator operator+(difference_type off) const noexcept
    {
        string_iterator tmp = *this;
        tmp += off;
        return tmp;
    }
    using base::operator-;  // difference_type operator-(const string_const_iterator& rhs)
    string_iterator operator-(difference_type off) const noexcept
    {
        string_iterator tmp = *this;
        tmp -= off;
        return tmp;
    }

    friend string_iterator operator+(difference_type off, string_iterator next) noexcept
    {
        next += off;
        return next;
    }

    reference operator[](difference_type off) const noexcept
    {
        return const_cast<reference>(base::operator[](off));
    }
};


/// 字符串
template<typename CharT, typename Traits = std::char_traits<CharT>, typename Alloc = std::allocator<CharT>,
    typename Storage = string_core<CharT, Traits>>
class basic_string
{
public:
    using traits_type = Traits;
    using value_type = CharT;
    using allocator_type = Alloc;
    using size_type = typename std::allocator_traits<Alloc>::size_type;
    using difference_type = typename std::allocator_traits<Alloc>::difference_type;

    using reference = value_type & ;
    using const_reference = const value_type&;
    using pointer = typename std::allocator_traits<Alloc>::pointer;
    using const_pointer = typename std::allocator_traits<Alloc>::const_pointer;

    //using iterator = value_type * ;
    using iterator = string_iterator<basic_string>;
    //using const_iterator = const value_type*;
    using const_iterator = string_const_iterator<basic_string>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    //! 字符串尾
    static constexpr size_type npos{ size_type(-1) };

private:
    inline static void checkSize(size_type& n, size_type nmax)
    {
        if (n > nmax) {
            n = nmax;
        }
    }

    template<typename Ex, typename... Args>
    inline static void enforce(bool condition, Args&&... args)
    {
        if (!condition) {
            throw Ex(static_cast<Args&&>(args)...);
        }
    }

public:  // Constructor
    basic_string() noexcept
        : basic_string(Alloc())
    {}

    explicit basic_string(const Alloc&) noexcept
    {}

    basic_string(size_type n, value_type c, const Alloc& = Alloc())
    {
        const auto data = store_.expandNoinit(n);
        traits_type::assign(data, n, c);
    }

    basic_string(const basic_string& rhs, size_type pos, size_type n = npos, const Alloc& = Alloc())
    {
        // rhs [pos, pos+n)
        assign(rhs, pos, n);
    }

    basic_string(const value_type* s, const Alloc& = Alloc())
        : store_(s, traits_type::length(s))
    {}

    basic_string(const value_type* s, size_type n, const Alloc& = Alloc())
        : store_(s, n)
    {}

    template<typename Iterator>
    basic_string(Iterator first, Iterator last,
        std::enable_if_t<!std::is_same<Iterator, value_type*>::value, const Alloc>& = Alloc())
    {
        assign(first, last);
    }

    /// Iterator == const char* 特化
    basic_string(const value_type* first, const value_type* last, const Alloc& = Alloc())
        : store_(first, size_type(last - first))
    {}

    basic_string(std::initializer_list<value_type> il)
    {
        assign(il.begin(), il.end());
    }

    basic_string(std::nullptr_t) = delete;

public:  // Copy construct、Move construct、operator=
    basic_string(const basic_string& rhs)
        : store_(rhs.store_)
    {}

    basic_string(basic_string&& rhs) noexcept
        : store_(std::move(rhs.store_))
    {}

    basic_string& operator=(const basic_string& rhs);

    basic_string& operator=(basic_string&& rhs) noexcept;

    basic_string& operator=(value_type ch);

    basic_string& operator=(const value_type* s)
    {
        return assign(s);
    }

    basic_string& operator=(std::initializer_list<value_type> il)
    {
        return assign(il.begin(), il.end());
    }

    basic_string& operator=(std::nullptr_t) = delete;

public:  // Destructor
    ~basic_string() noexcept
    {}

public:  // std::string
    template<typename Alloc2>
    basic_string(const std::basic_string<CharT, Traits, Alloc2>& str)
        : store_(str.data(), str.size())
    {}

    template<typename Alloc2>
    basic_string& operator=(const std::basic_string<CharT, Traits, Alloc2>& rhs)
    {
        return assign(rhs.data(), rhs.size());
    }

public:  // Iterators
    iterator begin() noexcept
    {
        return iterator(store_.mutableData());
    }
    iterator end() noexcept
    {
        return iterator(store_.mutableData() + store_.size());
    }

    const_iterator begin() const noexcept
    {
        return const_iterator(store_.data());
    }
    const_iterator end() const noexcept
    {
        return const_iterator(store_.data() + store_.size());
    }

    const_iterator cbegin() const noexcept
    {
        return begin();
    }
    const_iterator cend() const noexcept
    {
        return end();
    }

    reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }
    reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rbegin() const noexcept
    {
        return const_reverse_iterator(end());
    }
    const_reverse_iterator rend() const noexcept
    {
        return const_reverse_iterator(begin());
    }

    const_reverse_iterator crbegin() const noexcept
    {
        return rbegin();
    }
    const_reverse_iterator crend() const noexcept
    {
        return rend();
    }

public:  // Element access
    reference at(size_type pos)
    {
        enforce<std::out_of_range>(pos < size(), "out of range");
        return (*this)[pos];
    }
    const_reference at(size_type pos) const
    {
        enforce<std::out_of_range>(pos < size(), "out of range");
        return (*this)[pos];
    }

    reference operator[](size_type pos)
    {
        return *(begin() + pos);
    }
    const_reference operator[](size_type pos) const
    {
        return *(begin() + pos);
    }

    const value_type& front() const
    {
        return *begin();
    }
    const value_type& back() const
    {
        assert(!empty());
        return *(end() - 1);
    }

    value_type& front()
    {
        return *begin();
    }
    value_type& back()
    {
        assert(!empty());
        return *(end() - 1);
    }

    const value_type* data() const noexcept
    {
        return c_str();
    }
    value_type* data() noexcept
    {
        return store_.data();
    }
    const value_type* c_str() const noexcept
    {
        return store_.c_str();
    }

public:  // to std::string, base::string_view
#ifdef USE_HALCYON_STRING_VIEW
    operator basic_string_view<value_type, traits_type>() const noexcept
#else
    operator std::basic_string_view<value_type, traits_type>() const noexcept
#endif
    {
        return { data(), size() };
    }

    std::basic_string<CharT, Traits, Alloc> toStdString() const
    {
        return std::basic_string<CharT, Traits, Alloc>(data(), size());
    }

public:
    allocator_type get_allocator() const noexcept
    {
        return allocator_type();
    }

public:  // Capacity
    bool empty() const noexcept
    {
        return size() == 0;
    }

    size_type size() const noexcept
    {
        return store_.size();
    }

    size_type length() const noexcept
    {
        return size();
    }

    size_type max_size() const noexcept
    {
        return std::numeric_limits<size_type>::max();
    }

    size_type capacity() const noexcept
    {
        return store_.capacity();
    }

    /**
     * @brief   重新分配内存大小，用于扩大，不能缩小原有内存大小
     */
    void reserve(size_type new_cap)
    {
        enforce<std::length_error>(new_cap <= max_size(), "");
        store_.reserve(new_cap);
    }

    /**
     * @brief   收缩内存大小，仅当空余内存过大时收缩
     */
    void shrink_to_fit()
    {
        if (capacity() < size() * 3 / 2) {
            return;
        }
        basic_string(cbegin(), cend()).swap(*this);
    }

public:  // Operations
    void clear() noexcept
    {
        resize(0);
    }

    void push_back(value_type c)
    {
        store_.push_back(c);
    }

    void pop_back()
    {
        assert(!empty());
        store_.shrink(1);
    }

    void swap(basic_string& other) noexcept
    {
        store_.swap(other.store_);
    }

    /**
     * @brief       调整字符串的字符数，确保包含 n 个字符。若当前大小 < n，
     *          则扩充至 n，以 c 填充；若当前大小 > n，则缩减到 n 个元素
     * @param[in]   大小
     * @param[in]   扩充后，填充的字符
     */
    void resize(size_type n, value_type c = value_type());

public:  // Operations - assign
    basic_string& assign(const basic_string& str)
    {
        if (&str == this) {
            return *this;
        }
        return assign(str.data(), str.size());
    }

    basic_string& assign(basic_string&& str)
    {
        return *this = std::move(str);
    }

    basic_string& assign(const basic_string& str, size_type pos, size_type n = npos);

    basic_string& assign(std::initializer_list<value_type> il)
    {
        return assign(il.begin(), il.end());
    }

    basic_string& assign(const value_type* s)
    {
        return assign(s, traits_type::length(s));
    }
    basic_string& assign(const value_type* s, size_type n);

    // 合并以下两个函数
    // assign(size_type n, value_type c);
    // assign(Iterator first, Iterator last);
    template<typename IterOrLength, typename IterOrChar, 
        typename = std::enable_if_t<(is_iterator<IterOrLength>::value && is_iterator<IterOrChar>::value)
        || (std::is_same<IterOrChar, value_type>::value)>>
    basic_string& assign(IterOrLength first_or_n, IterOrChar last_or_c)
    {
        return replace(begin(), end(), first_or_n, last_or_c);
    }

public:  // Operations - insert
    basic_string& insert(size_type pos, const value_type* s)
    {
        // insert(size_type pos, const value_type* s, size_type n)
        return insert(pos, s, traits_type::length(s));
    }
    basic_string& insert(size_type pos, const value_type* s, size_type n)
    {
        enforce<std::out_of_range>(pos <= size(), "invalid string position");
        // 模板 insert(const_iterator p, Iterator first, Iterator last)
        insert(begin() + pos, s, s + n);
        return *this;
    }

    basic_string& insert(size_type pos1, const basic_string& str)
    {
        // insert(size_type pos, const value_type* s, size_type n)
        return insert(pos1, str.data(), str.size());
    }
    basic_string& insert(size_type pos1, const basic_string& str, 
        size_type pos2, size_type n = npos)
    {
        enforce<std::out_of_range>(pos2 <= str.size(), "invalid string position");
        checkSize(n, str.length() - pos2);
        // insert(size_type pos, const value_type* s, size_type n)
        return insert(pos1, str.data() + pos2, n);
    }

    basic_string& insert(size_type pos, size_type n, value_type c)
    {
        enforce<std::out_of_range>(pos <= size(), "invalid string position");
        // 模板 insert(const_iterator p, size_type n, value_type c)
        insert(begin() + pos, n, c);
        return *this;
    }

    iterator insert(const_iterator p, std::initializer_list<value_type> il)
    {
        // 模板 insert(const_iterator p, Iterator first, Iterator last)
        return insert(p, il.begin(), il.end());
    }
    
    iterator insert(const_iterator p, value_type c)
    {
        const size_type pos = p - cbegin();
        // 模板 insert(const_iterator p, size_type n, value_type c)
        insert(p, 1, c);
        return begin() + pos;
    }

    // 合并以下两个函数
    // insert(const_iterator p, size_type n, value_type c);
    // insert(const_iterator p, Iterator first, Iterator last);
    template<typename IterOrLength, typename IterOrChar,
        typename = std::enable_if_t<(is_iterator<IterOrLength>::value&& is_iterator<IterOrChar>::value)
        || (std::is_same<IterOrChar, value_type>::value/* && std::is_same<IterOrLength, size_type>::value*/)>>
    iterator insert(const_iterator p, IterOrLength first_or_n, IterOrChar last_or_c)
    {
        // std::numeric_limits<T>::is_specialized 的值对所有存在 std::numeric_limits 特化的 T 为 true
        // 即判断 IterOrLength 是迭代器（flase）还是整数（true）
        using Sel = std::bool_constant<std::numeric_limits<IterOrLength>::is_specialized>;
        return insertImpl(p, first_or_n, last_or_c, Sel());
    }

public:  // Operations - append
    basic_string& append(const basic_string& str);
    basic_string& append(const basic_string& str, size_type pos, size_type n = npos);

    basic_string& append(const value_type* s)
    {
        return append(s, traits_type::length(s));
    }
    basic_string& append(const value_type* s, size_type n);

    basic_string& append(size_type n, value_type c);

    template<typename Iterator>
    basic_string& append(Iterator first, Iterator last)
    {
        insert(this->end(), first, last);
        return *this;
    }

    basic_string& append(std::initializer_list<value_type> il)
    {
        return append(il.begin(), il.end());
    }

public:  // Operations - operator+=
    basic_string& operator+=(const basic_string& str)
    {
        return append(str);
    }
    basic_string& operator+=(const value_type* s)
    {
        return append(s);
    }
    basic_string& operator+=(value_type c)
    {
        push_back(c);
        return *this;
    }
    basic_string& operator+=(std::initializer_list<value_type> il)
    {
        return append(il);
    }

    template<typename Alloc2>
    basic_string& operator+=(const std::basic_string<CharT, Traits, Alloc2>& str)
    {
        return append(str.data(), str.size());
    }

#ifdef USE_HALCYON_STRING_VIEW
    basic_string& operator+=(basic_string_view<value_type, traits_type> str)
#else
    basic_string& operator+=(std::basic_string_view<value_type, traits_type> str)
#endif
    {
        return append(str.data(), str.size());
    }

public:  // Operations - erase
    iterator erase(const_iterator position)
    {
        const size_type pos(position - begin());
        erase(pos, 1);
        return begin() + pos;
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        const size_type pos(first - this->begin());
        erase(pos, last - first);
        return begin() + pos;
    }
    basic_string& erase(size_type pos = 0, size_type n = npos)
    {
        enforce<std::out_of_range>(pos <= size(), "invalid string position");
        checkSize(n, size() - pos);
        std::copy(begin() + pos + n, end(), begin() + pos);
        resize(size() - n);
        return *this;
    }

public:  // Operations - replace
    basic_string& replace(size_type pos, size_type n, const basic_string& str)
    {
        return replace(pos, n, str.data(), str.size());
    }
    basic_string& replace(size_type pos1, size_type n1, const basic_string& str,
        size_type pos2, size_type n2 = npos)
    {
        enforce<std::out_of_range>(pos2 <= str.size(), "invalid string position");
        return replace(pos1, n1, str.data() + pos2, std::min(n2, str.size() - pos2));
    }

    basic_string& replace(size_type pos, size_type n, const value_type* s)
    {
        return replace(pos, n, s, traits_type::length(s));
    }

    basic_string& replace(size_type pos, size_type n1, const value_type* s, size_type n2)
    {
        enforce<std::out_of_range>(pos <= size(), "invalid string position");
        checkSize(n1, size() - pos);
        const_iterator first = begin() + pos;
        // 模板 replace(const_iterator, const_iterator, const value_type*, size_type);
        return replace(first, first + n1, s, n2);
    }

    basic_string& replace(size_type pos, size_type n1, size_type n2, value_type c)
    {
        enforce<std::out_of_range>(pos <= size(), "invalid string position");
        checkSize(n1, size() - pos);
        const_iterator first = begin() + pos;
        // 模板 replace(const_iterator, const_iterator, size_type, value_type);
        return replace(first, first + n1, n2, c);
    }

    basic_string& replace(const_iterator first, const_iterator last, const basic_string& str)
    {
        // 模板 replace(const_iterator, const_iterator, const value_type*, size_type)
        return replace(first, last, str.data(), str.size());
    }

    basic_string& replace(const_iterator first, const_iterator last, const value_type* s)
    {
        // 模板 replace(const_iterator, const_iterator, const value_type*, size_type)
        return replace(first, last, s, traits_type::length(s));
    }

    basic_string& replace(const_iterator first, const_iterator last, std::initializer_list<value_type> il)
    {
        // 模板 replace(const_iterator, const_iterator, Iterator, Iterator)
        return replace(first, last, il.begin(), il.end());
    }

    // 合并以下三个函数
    // template<typename Iterator>
    // replace(const_iterator first, const_iterator last, Iterator first2, Iterator last2);
    // replace(const_iterator first, const_iterator last, const value_type* s, size_type n2);
    // replace(const_iterator first, const_iterator last, size_type n2, value_type c);
    template<typename T1, typename T2>
    basic_string& replace(const_iterator first, const_iterator last, T1 first2_or_s_or_n2, T2 last2_or_n2_or_c)
    {
        constexpr bool num1 = std::numeric_limits<T1>::is_specialized;
        constexpr bool num2 = std::numeric_limits<T2>::is_specialized;

        // 判断使用哪个 replaceImpl  (first2, last2)->0  (n2, c)->1  (s, n2)->2
        using Sel = std::integral_constant<int, num1 ? (num2 ? 1 : -1) : (num2 ? 2 : 0)>;
        return replaceImpl(first, last, first2_or_s_or_n2, last2_or_n2_or_c, Sel());
    }

public:  // Operations - compare
    int compare(const basic_string& str) const noexcept
    {
        return compare(0, size(), str);
    }
    int compare(size_type pos1, size_type n1, const basic_string& str) const
    {
        return compare(pos1, n1, str.data(), str.size());
    }
    int compare(size_type pos1, size_type n1, const basic_string& str, size_type pos2, size_type n2 = npos) const
    {
        enforce<std::out_of_range>(pos2 <= str.size(), "invalid string position");
        return compare(pos1, n1, str.data() + pos2, std::min(n2, str.size() - pos2));
    }

    int compare(size_type pos1, size_type n1, const value_type* s) const
    {
        return compare(pos1, n1, s, traits_type::length(s));
    }
    int compare(size_type pos1, size_type n1, const value_type* s, size_type n2) const
    {
        enforce<std::out_of_range>(pos1 <= size(), "invalid string position");
        checkSize(n1, size() - pos1);
        const int res = traits_type::compare(data() + pos1, s, std::min(n1, n2));
        return res != 0 ? res : n1 > n2 ? 1 : n1 < n2 ? -1 : 0;
    }

    int compare(const value_type* s) const
    {
        const size_type n1(size()), n2(traits_type::length(s));
        const int res = traits_type::compare(data(), s, std::min(n1, n2));
        return res != 0 ? res : n1 > n2 ? 1 : n1 < n2 ? -1 : 0;
    }

public:  // Operations - substr
    /**
     * @brief       获取子串
     * @param[in]   起始位置
     * @param[in]   子串长度
     */
    basic_string substr(size_type pos = 0, size_type n = npos) const&
    {
        enforce<std::out_of_range>(pos <= size(), "invalid string position");
        return basic_string(data() + pos, std::min(n, size() - pos));
    }
    
    /*
     * @brief       获取子串(原先字符串失效)
     * @param[in]   起始位置
     * @param[in]   子串长度
     * @ps          右值调用
     */
    basic_string substr(size_type pos = 0, size_type n = npos) &&
    {
        enforce<std::out_of_range>(pos <= size(), "");
        erase(0, pos);
        if (n < size()) {
            resize(n);
        }
        return std::move(*this);
    }

public:  // Operations - starts_with, ends_with
    bool starts_with(value_type c) const noexcept
    {
        return !empty() && traits_type::eq(c, front());
    }
    bool starts_with(const value_type* s) const
    {
        const size_type n = traits_type::length(s);
        return size() >= n && traits_type::compare(data(), s, n) == 0;
    }

#ifdef USE_HALCYON_STRING_VIEW
    bool starts_with(basic_string_view<value_type, traits_type> sv) const noexcept
#else
    bool starts_with(std::basic_string_view<value_type, traits_type> sv) const noexcept
#endif
    {
        return size() >= sv.size() && traits_type::compare(data(), sv.data(), sv.size()) == 0;
    }

    bool ends_with(value_type c) const noexcept
    {
        return !empty() && traits_type::eq(c, back());
    }
    bool ends_with(const value_type* s) const
    {
        const size_type n = traits_type::length(s);
        return size() >= n && traits_type::compare(data() + size() - n, s, n) == 0;
    }

#ifdef USE_HALCYON_STRING_VIEW
    bool ends_with(basic_string_view<value_type, traits_type> sv) const noexcept
#else
    bool ends_with(std::basic_string_view<value_type, traits_type> sv) const noexcept
#endif
    {
        return size() >= sv.size() && traits_type::compare(data() + size() - sv.size(), sv.data(), sv.size()) == 0;
    }

public:  // Operations - contains
    bool contains(value_type c) const noexcept
    {
        return find(c) != npos;
    }
    bool contains(const value_type* s) const
    {
        return find(s) != npos;
    }
#ifdef USE_HALCYON_STRING_VIEW
    bool contains(basic_string_view<value_type, traits_type> sv) const noexcept
#else
    bool contains(std::basic_string_view<value_type, traits_type> sv) const noexcept
#endif
    {
        return find(sv.data(), 0, sv.size()) != npos;
    }

public:  // Operations - find
    size_type find(value_type c, size_type pos = 0) const noexcept
    {
        return find(&c, pos, 1);
    }
    size_type find(const value_type* s, size_type pos = 0) const
    {
        return find(s, pos, traits_type::length(s));
    }
    size_type find(const basic_string& str, size_type pos = 0) const noexcept
    {
        return find(str.data(), pos, str.size());
    }
    /*
     * @brief       查找
     * @param[in]   要搜索的子串
     * @param[in]   要开始搜索的位置
     * @param[in]   要搜索的子串长度
     */
    size_type find(const value_type* s, size_type pos, size_type n) const;

public:  // Operations - rfind
    size_type rfind(value_type c, size_type pos = npos) const noexcept
    {
        return rfind(&c, pos, 1);
    }
    size_type rfind(const value_type* s, size_type pos = npos) const
    {
        return rfind(s, pos, traits_type::length(s));
    }
    size_type rfind(const basic_string& str, size_type pos = npos) const noexcept
    {
        return rfind(str.data(), pos, str.size());
    }
    size_type rfind(const value_type* s, size_type pos, size_type n) const;

public:  // Operations - find_first_of
    size_type find_first_of(value_type c, size_type pos = 0) const noexcept
    {
        return find_first_of(&c, pos, 1);
    }
    size_type find_first_of(const value_type* s, size_type pos = 0) const
    {
        return find_first_of(s, pos, traits_type::length(s));
    }
    size_type find_first_of(const basic_string& str, size_type pos = 0) const noexcept
    {
        return find_first_of(str.data(), pos, str.size());
    }
    size_type find_first_of(const value_type* s, size_type pos, size_type n) const;

public:  // Operations - find_first_not_of
    size_type find_first_not_of(value_type c, size_type pos = 0) const noexcept
    {
        return find_first_not_of(&c, pos, 1);
    }
    size_type find_first_not_of(const value_type* s, size_type pos = 0) const
    {
        return find_first_not_of(s, pos, traits_type::length(s));
    }
    size_type find_first_not_of(const basic_string& str, size_type pos = 0) const noexcept
    {
        return find_first_not_of(str.data(), pos, str.size());
    }
    size_type find_first_not_of(const value_type* s, size_type pos, size_type n) const;

public:  // Operations - find_last_of
    size_type find_last_of(value_type c, size_type pos = npos) const noexcept
    {
        return find_last_of(&c, pos, 1);
    }
    size_type find_last_of(const value_type* s, size_type pos = npos) const
    {
        return find_last_of(s, pos, traits_type::length(s));
    }
    size_type find_last_of(const basic_string& str, size_type pos = npos) const noexcept
    {
        return find_last_of(str.data(), pos, str.size());
    }
    size_type find_last_of(const value_type* s, size_type pos, size_type n) const;

public:  // Operations - find_last_not_of
    size_type find_last_not_of(value_type c, size_type pos = npos) const noexcept
    {
        return find_last_not_of(&c, pos, 1);
    }
    size_type find_last_not_of(const value_type* s, size_type pos = npos) const
    {
        return find_last_not_of(s, pos, traits_type::length(s));
    }
    size_type find_last_not_of(const basic_string& str, size_type pos = npos) const noexcept
    {
        return find_last_not_of(str.data(), pos, str.size());
    }
    size_type find_last_not_of(const value_type* s, size_type pos, size_type n) const;

public:
    /*
     * @brief       复制字符（复制到 dest 中）
     * @param[in]   指向目标字符串的指针
     * @param[in]   子串长度
     * @param[in]   包含的首字符位置
     * @return      复制的字符数
     * @ps          复制产生的字符串不是空终止的
     */
    size_type copy(value_type* dest, size_type n, size_type pos = 0) const
    {
        enforce<std::out_of_range>(pos <= size(), "");
        checkSize(n, size() - pos);
        if (n != 0) {
            traits_type::copy(dest, data() + pos, n);
        }
        return n;
    }

    /*
     * @brief       转大写
     */
    basic_string& upper()
    {
        std::transform(begin(), end(), begin(), ::toupper);
        return *this;
    }

    /*
     * @brief       转小写
     */
    basic_string& lower()
    {
        std::transform(begin(), end(), begin(), ::tolower);
        return *this;
    }

    //比较字符串（忽略大小小写）
    int compare_no_case() const;

    basic_string& trim_left();
    basic_string& trim_right();
    basic_string& trim();

    size_type find_no_case() const;

private:  // insert impl
    iterator insertImpl(const_iterator pos, size_type n, value_type c, std::true_type);

    template<typename Iterator>
    iterator insertImpl(const_iterator pos, Iterator first, Iterator last, std::false_type);

private:  // replace impl
    template<typename Iterator>
    basic_string& replaceImpl(const_iterator i1, const_iterator i2, Iterator b, Iterator e, std::integral_constant<int, 0>);
    basic_string& replaceImpl(const_iterator i1, const_iterator i2, size_type n2, value_type c, std::integral_constant<int, 1>);
    basic_string& replaceImpl(const_iterator i1, const_iterator i2, const value_type* s, size_type n, std::integral_constant<int, 2>);

    template<typename Iterator>
    bool replaceAliased(const_iterator, const_iterator, Iterator, Iterator, std::false_type)
    {
        return false;
    }
    
    template<typename Iterator>
    bool replaceAliased(const_iterator i1, const_iterator i2, Iterator s1, Iterator s2, std::true_type);

private:
    //! 字符串的存储
    Storage store_;
};

template<typename C, typename T, typename A, typename S>
constexpr typename basic_string<C, T, A, S>::size_type basic_string<C, T, A, S>::npos;

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S>& basic_string<C, T, A, S>::operator=(const basic_string& rhs)
{
    if (&rhs == this) {
        return *this;
    }
    return assign(rhs.data(), rhs.size());
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S>& basic_string<C, T, A, S>::operator=(basic_string&& rhs) noexcept
{
    if (&rhs == this) {
        return *this;
    }
    this->~basic_string();
    new (&store_) S(std::move(rhs.store_));
    return *this;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S>& basic_string<C, T, A, S>::operator=(value_type c)
{
    if (empty()) {
        store_.expandNoinit(1);
    } else if (store_.isShared()) {
        basic_string(1, c).swap(*this);
        return *this;
    } else {
        store_.shrink(size() - 1);
    }
    front() = c;
    return *this;
}

template<typename C, typename T, typename A, typename S>
inline void basic_string<C, T, A, S>::resize(size_type n, value_type c)
{
    size_type size = this->size();
    if (n <= size) {
        // 字符串长度缩小
        store_.shrink(size - n);
    } else {
        // 字符串长度扩大
        const size_type delta = n - size;
        auto data = store_.expandNoinit(delta);
        traits_type::assign(data, delta, c);
    }
    assert(this->size() == n);
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S>& basic_string<C, T, A, S>::assign(const basic_string& str, size_type pos, size_type n)
{
    const size_type size = str.size();
    enforce<std::out_of_range>(pos <= size, "invalid string position");
    checkSize(n, size - pos);
    return assign(str.data() + pos, n);
}

template<typename C, typename T, typename A, typename S>
basic_string<C, T, A, S>& basic_string<C, T, A, S>::assign(const value_type* s, size_type n)
{
    if (n == 0) {
        resize(0);
    } else if (size() >= n) {
        traits_type::move(store_.mutableData(), s, n);
        store_.shrink(size() - n);
        assert(size() == n);
    } else {
        resize(0);
        traits_type::copy(store_.expandNoinit(n), s, n);
    }
    assert(size() == n);
    return *this;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S>& basic_string<C, T, A, S>::append(const basic_string& str)
{
    size_type expect_size = size() + str.size();
    append(str.data(), str.size());
    assert(expect_size == size());
    return *this;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S>& basic_string<C, T, A, S>::append(const basic_string& str, size_type pos, size_type n)
{
    const size_type size = str.size();
    enforce<std::out_of_range>(pos <= size, "invalid string position");
    checkSize(n, size - pos);
    return append(str.data() + pos, n);
}

template<typename C, typename T, typename A, typename S>
basic_string<C, T, A, S>& basic_string<C, T, A, S>::append(const value_type* s, size_type n)
{
    if (n == 0) {
        return *this;
    }
    const size_type old_size = size();
    const auto old_data = this->data();
    auto data = store_.expandNoinit(n, true);

    std::less_equal<const value_type*> le;
    if (le(old_data, s) && !(le(old_data + old_size, s))) {
        assert(le(s + n, old_data + old_size));
        s = this->data() + (s - old_data);
        traits_type::move(data, s, n);
    } else {
        traits_type::copy(data, s, n);
    }
    assert(size() == old_size + n);
    return *this;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S>& basic_string<C, T, A, S>::append(size_type n, value_type c)
{
    auto data = store_.expandNoinit(n, true);
    traits_type::assign(data, n, c);
    return *this;
}

template<typename C, typename T, typename A, typename S>
inline typename basic_string<C, T, A, S>::size_type basic_string<C, T, A, S>::find(
    const value_type* s, size_type pos, size_type n) const
{
    const size_type size = this->size();
    if (n + pos > size || n + pos < pos) {
        return npos;
    }

    if (n == 0) {
        return pos;
    }

    const value_type* data = this->data();

    const size_type n1 = n - 1;
    const value_type lastChar = s[n1];

    size_type skip = 0;

    const value_type* i = data + pos;
    const value_type* end = data + size - n1;

    while (i < end) {
        // 先对比最后一个字符
        while (i[n1] != lastChar) {
            if (++i == end) {
                return npos;
            }
        }

        for (size_t j = 0;;) {
            assert(j < n);
            if (i[j] != s[j]) {
                // 不相等的情况，移动量=不匹配的后缀
                // 原: 1111112313        1111112313        1111112313
                //                 -> 一             -> 二 (skip=2)
                // 搜: 12313                12313               12313
                // 尾字符 3 重复出现
                if (skip == 0) {
                    skip = 1;
                    while (skip <= n1 && s[n1 - skip] != lastChar) {
                        ++skip;
                    }
                }
                i += skip;
                break;
            }

            if (++j == n) {
                // 找到
                return i - data;
            }
        }
    }
    return npos;
}

template<typename C, typename T, typename A, typename S>
typename basic_string<C, T, A, S>::size_type basic_string<C, T, A, S>::rfind(
    const value_type* s, size_type pos, size_type n) const
{
    const size_type size = this->size();
    if (n > size) {
        return npos;
    }
    pos = std::min(pos, size - n);
    if (n == 0) {
        return pos;
    }

    const_iterator i(begin() + pos);
    for (;; --i) {
        if (traits_type::eq(*i, *s) && traits_type::compare(&*i, s, n) == 0) {
            return i - begin();
        }
        if (i == begin()) {
            break;
        }
    }
    return npos;
}

template<typename C, typename T, typename A, typename S>
typename basic_string<C, T, A, S>::size_type basic_string<C, T, A, S>::find_first_of(
    const value_type* s, size_type pos, size_type n) const
{
    if (pos < size() && n != 0) {
        const_iterator i(begin() + pos), finish(end());
        for (; i != finish; ++i) {
            // 判断字符 i 是否在 s 中
            if (traits_type::find(s, n, *i) != nullptr) {
                return i - begin();
            }
        }
    }
    return npos;
}

template<typename C, typename T, typename A, typename S>
typename basic_string<C, T, A, S>::size_type basic_string<C, T, A, S>::find_first_not_of(
    const value_type* s, size_type pos, size_type n) const
{
    if (pos < size()) {
        const_iterator i(begin() + pos), finish(end());
        for (; i != finish; ++i) {
            if (traits_type::find(s, n, *i) == nullptr) {
                return i - begin();
            }
        }
    }
    return npos;
}

template<typename C, typename T, typename A, typename S>
typename basic_string<C, T, A, S>::size_type basic_string<C, T, A, S>::find_last_of(
    const value_type* s, size_type pos, size_type n) const
{
    if (!empty() && n != 0) {
        pos = std::min(pos, size() - 1);
        for (const_iterator i = begin() + pos; ; --i) {
            if (traits_type::find(s, n, *i) != nullptr) {
                return i - begin();
            }
            if (i == begin()) {
                break;
            }
        }
    }
    return npos;
}

template<typename C, typename T, typename A, typename S>
typename basic_string<C, T, A, S>::size_type basic_string<C, T, A, S>::find_last_not_of(
    const value_type* s, size_type pos, size_type n) const
{
    if (!empty()) {
        pos = std::min(pos, size() - 1);
        for (const_iterator i = begin() + pos; ; --i) {
            if (traits_type::find(s, n, *i) == nullptr) {
                return i - begin();
            }
            if (i == begin()) {
                break;
            }
        }
    }
    return npos;
}

template<typename C, typename T, typename A, typename S>
inline typename basic_string<C, T, A, S>::iterator basic_string<C, T, A, S>::insertImpl(
    const_iterator p, size_type n, value_type c, std::true_type)
{
    assert(p >= cbegin() && p <= cend());
    const size_type pos = p - cbegin();

    size_type old_size = size();
    store_.expandNoinit(n, true);
    auto b = const_cast<pointer>(begin().operator->());
    traits_type::move(b + pos + n, b + pos, old_size - pos);
    traits_type::assign(b + pos, n, c);
    return iterator(b + pos);
}

template<typename C, typename T, typename A, typename S>
template<typename Iterator>
inline typename basic_string<C, T, A, S>::iterator basic_string<C, T, A, S>::insertImpl(
    const_iterator p, Iterator first, Iterator last, std::false_type)
{
    assert(p >= cbegin() && p <= cend());
    const size_type pos = p - cbegin();
    auto n = std::distance(first, last);
    assert(n >= 0);

    size_type old_size = size();
    store_.expandNoinit(n, true);
    auto b = const_cast<pointer>(begin().operator->());
    traits_type::move(b + pos + n, b + pos, old_size - pos);
    std::copy(first, last, b + pos);
    return iterator(b + pos);
}

template<typename C, typename T, typename A, typename S>
template<typename Iterator>
inline basic_string<C, T, A, S>& basic_string<C, T, A, S>::replaceImpl(
    const_iterator i1, const_iterator i2, Iterator s1, Iterator s2, std::integral_constant<int, 0>)
{
    // Handle aliased replace
    using Sel = std::bool_constant<std::is_same<Iterator, iterator>::value
        || std::is_same<Iterator, const_iterator>::value>;

    if (replaceAliased(i1, i2, s1, s2, Sel())) {
        return *this;
    }

    const auto n1 = i2 - i1;
    assert(n1 >= 0);
    const auto n2 = std::distance(s1, s2);
    assert(n2 >= 0);

    pointer b = const_cast<pointer>(i1.operator->());
    if (n1 > n2) {
        std::copy(s1, s2, b);
        erase(i1 + n2, i2);
    }
    else {
        s1 = detail::copy_n(s1, n1, b).first;
        insert(i2, s1, s2);
    }
    return *this;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S>& basic_string<C, T, A, S>::replaceImpl(
    const_iterator i1, const_iterator i2, size_type n2, value_type c, std::integral_constant<int, 1>)
{
    const size_type n1 = i2 - i1;
    pointer begin = const_cast<pointer>(i1.operator->());
    if (n1 > n2) {
        std::fill(begin, begin + n2, c);
        erase(i1 + n2, i2);
    } else {
        pointer end = const_cast<pointer>(i2.operator->());
        std::fill(begin, end, c);
        insert(i2, n2 - n1, c);
    }
    return *this;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S>& basic_string<C, T, A, S>::replaceImpl(
    const_iterator i1, const_iterator i2, const value_type* s, size_type n, std::integral_constant<int, 2>)
{
    assert(i1 <= i2);
    assert(begin() <= i1 && i1 <= end());
    assert(begin() <= i2 && i2 <= end());
    return replace(i1, i2, s, s + n);
}

template<typename C, typename T, typename A, typename S>
template<typename FwdIterator>
inline bool basic_string<C, T, A, S>::replaceAliased(
    const_iterator i1, const_iterator i2, FwdIterator s1, FwdIterator s2, std::true_type)
{
    std::less_equal<const value_type*> le{};
    const bool aliased = le(&*begin(), &*s1) && le(&*s1, &*end());
    if (!aliased) {
        return false;
    }
    // Aliased replace, copy to new string
    basic_string tmp;
    tmp.reserve(size() - (i2 - i1) + std::distance(s1, s2));
    tmp.append(cbegin(), i1).append(s1, s2).append(i2, cend());
    swap(tmp);
    return true;
}



/// non-member functions
// operator+
template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(const basic_string<C, T, A, S>& lhs, const basic_string<C, T, A, S>& rhs)
{
    basic_string<C, T, A, S> result;
    result.reserve(lhs.size() + rhs.size());
    result.append(lhs).append(rhs);
    return result;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(basic_string<C, T, A, S>&& lhs, const basic_string<C, T, A, S>& rhs)
{
    return std::move(lhs.append(rhs));
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(const basic_string<C, T, A, S>& lhs, basic_string<C, T, A, S>&& rhs)
{
    if (rhs.capacity() >= lhs.size() + rhs.size()) {
        // no need reallocate
        return std::move(rhs.insert(0, lhs));
    }

    auto const& rhsC = rhs;
    return lhs + rhsC;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(basic_string<C, T, A, S>&& lhs, basic_string<C, T, A, S>&& rhs)
{
    return std::move(lhs.append(rhs));
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(const C* lhs, const basic_string<C, T, A, S>& rhs)
{
    basic_string<C, T, A, S> result;
    const auto len = basic_string<C, T, A, S>::traits_type::length(lhs);
    result.reserve(len + rhs.size());
    result.append(lhs, len).append(rhs);
    return result;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(const C* lhs, basic_string<C, T, A, S>&& rhs)
{
    const auto len = basic_string<C, T, A, S>::traits_type::length(lhs);
    if (rhs.capacity() >= len + rhs.size()) {
        rhs.insert(rhs.begin(), lhs, lhs + len);
        return std::move(rhs);
    }
    basic_string<C, T, A, S> result;
    result.reserve(len + rhs.size());
    result.append(lhs, len).append(rhs);
    return result;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(const C lhs, const basic_string<C, T, A, S>& rhs)
{
    basic_string<C, T, A, S> result;
    result.reserve(1 + rhs.size());
    result.push_back(lhs);
    result.append(rhs);
    return result;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(const C lhs, basic_string<C, T, A, S>&& rhs)
{
    if (rhs.capacity() > rhs.size()) {
        rhs.insert(rhs.begin(), lhs);
        return std::move(rhs);
    }
    auto const& rhsC = rhs;
    return lhs + rhsC;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(const basic_string<C, T, A, S>& lhs, const C* rhs)
{
    basic_string<C, T, A, S> result;
    const auto len = basic_string<C, T, A, S>::traits_type::length(rhs);
    result.reserve(lhs.size() + len);
    result.append(lhs).append(rhs, len);
    return result;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(basic_string<C, T, A, S>&& lhs, const C* rhs)
{
    return std::move(lhs += rhs);
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(const basic_string<C, T, A, S>& lhs, C rhs)
{
    basic_string<C, T, A, S> result;
    result.reserve(lhs.size() + 1);
    result.append(lhs);
    result.push_back(rhs);
    return result;
}

template<typename C, typename T, typename A, typename S>
inline basic_string<C, T, A, S> operator+(basic_string<C, T, A, S>&& lhs, C rhs)
{
    return std::move(lhs += rhs);
}

// operator==
template<typename C, typename T, typename A, typename S>
inline bool operator==(const basic_string<C, T, A, S>& lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}

template<typename C, typename T, typename A, typename S>
inline bool operator==(const basic_string<C, T, A, S>& lhs, const typename basic_string<C, T, A, S>::value_type* rhs) noexcept
{
    return lhs.compare(rhs) == 0;
}

template<typename C, typename T, typename A, typename S>
inline bool operator==(const typename basic_string<C, T, A, S>::value_type* lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return rhs == lhs;
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator==(const basic_string<C, T, A, S>& lhs, const std::basic_string<C, T, A2>& rhs)
{
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) == 0;
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator==(const std::basic_string<C, T, A2>& lhs, const basic_string<C, T, A, S>& rhs)
{
    return rhs == lhs;
}

// operator!=
template<typename C, typename T, typename A, typename S>
inline bool operator!=(const basic_string<C, T, A, S>& lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename C, typename T, typename A, typename S>
inline bool operator!=(const basic_string<C, T, A, S>& lhs, const typename basic_string<C, T, A, S>::value_type* rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename C, typename T, typename A, typename S>
inline bool operator!=(const typename basic_string<C, T, A, S>::value_type* lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator!=(const basic_string<C, T, A, S>& lhs, const std::basic_string<C, T, A2>& rhs)
{
    return !(lhs == rhs);
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator!=(const std::basic_string<C, T, A2>& lhs, const basic_string<C, T, A, S>& rhs)
{
    return !(lhs == rhs);
}

// operator<
template<typename C, typename T, typename A, typename S>
inline bool operator<(const basic_string<C, T, A, S>& lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return lhs.compare(rhs) < 0;
}

template<typename C, typename T, typename A, typename S>
inline bool operator<(const basic_string<C, T, A, S>& lhs, const typename basic_string<C, T, A, S>::value_type* rhs) noexcept
{
    return lhs.compare(rhs) < 0;
}

template<typename C, typename T, typename A, typename S>
inline bool operator<(const typename basic_string<C, T, A, S>::value_type* lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return rhs.compare(lhs) > 0;
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator<(const basic_string<C, T, A, S>& lhs, const std::basic_string<C, T, A2>& rhs)
{
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) < 0;
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator<(const std::basic_string<C, T, A2>& lhs, const basic_string<C, T, A, S>& rhs)
{
    return rhs > lhs;
}

// operator>
template<typename C, typename T, typename A, typename S>
inline bool operator>(const basic_string<C, T, A, S>& lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return rhs < lhs;
}

template<typename C, typename T, typename A, typename S>
inline bool operator>(const basic_string<C, T, A, S>& lhs, const typename basic_string<C, T, A, S>::value_type* rhs) noexcept
{
    return rhs < lhs;
}

template<typename C, typename T, typename A, typename S>
inline bool operator>(const typename basic_string<C, T, A, S>::value_type* lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return rhs < lhs;
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator>(const basic_string<C, T, A, S>& lhs, const std::basic_string<C, T, A2>& rhs)
{
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) > 0;
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator>(const std::basic_string<C, T, A2>& lhs, const basic_string<C, T, A, S>& rhs)
{
    return rhs < lhs;
}

// operator<=
template<typename C, typename T, typename A, typename S>
inline bool operator<=(const basic_string<C, T, A, S>& lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return !(rhs < lhs);
}

template<typename C, typename T, typename A, typename S>
inline bool operator<=(const basic_string<C, T, A, S>& lhs, const typename basic_string<C, T, A, S>::value_type* rhs) noexcept
{
    return !(rhs < lhs);
}

template<typename C, typename T, typename A, typename S>
inline bool operator<=(const typename basic_string<C, T, A, S>::value_type* lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return !(rhs < lhs);
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator<=(const basic_string<C, T, A, S>& lhs, const std::basic_string<C, T, A2>& rhs)
{
    return !(lhs > rhs);
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator<=(const std::basic_string<C, T, A2>& lhs, const basic_string<C, T, A, S>& rhs)
{
    return !(lhs > rhs);
}

// operator>=
template<typename C, typename T, typename A, typename S>
inline bool operator>=(const basic_string<C, T, A, S>& lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return !(lhs < rhs);
}

template<typename C, typename T, typename A, typename S>
inline bool operator>=(const basic_string<C, T, A, S>& lhs, const typename basic_string<C, T, A, S>::value_type* rhs) noexcept
{
    return !(lhs < rhs);
}

template<typename C, typename T, typename A, typename S>
inline bool operator>=(const typename basic_string<C, T, A, S>::value_type* lhs, const basic_string<C, T, A, S>& rhs) noexcept
{
    return !(lhs < rhs);
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator>=(const basic_string<C, T, A, S>& lhs, const std::basic_string<C, T, A2>& rhs)
{
    return !(lhs < rhs);
}

template<typename C, typename T, typename A, typename S, typename A2>
inline bool operator>=(const std::basic_string<C, T, A2>& lhs, const basic_string<C, T, A, S>& rhs)
{
    return !(lhs < rhs);
}


template<typename C, typename T, typename A, typename S>
inline void swap(basic_string<C, T, A, S>& lhs, basic_string<C, T, A, S>& rhs) noexcept
{
    lhs.swap(rhs);
}

using string = basic_string<char>;

BASE_END_NAMESPACE

namespace std
{
    template<>
    struct hash<::halcyon::base::basic_string<char>>
    {
        size_t operator()(const ::halcyon::base::basic_string<char>& s) const
        {
            return ::halcyon::base::genHashFunction(s.data(), static_cast<int>(s.size()));
        }
    };
}