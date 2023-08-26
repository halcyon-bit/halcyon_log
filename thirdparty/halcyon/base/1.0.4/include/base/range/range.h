#ifndef BASE_RANGE_H
#define BASE_RANGE_H

#include <base/common/base_define.h>

#include <stdexcept>

BASE_BEGIN_NAMESPACE

/**
 * @brief   Range 类, Range(1, 10) -> [1, 2, 3, 4, 5, 6, 7, 8, 9]
 * @ps      主要用于基于范围的 for 循环上
 */
template<typename T>
class RangeImpl
{
    // 迭代器
    template<typename U>
    class Iterator;

public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using iterator = Iterator<T>;
    using const_iterator = const Iterator<T>;
    using size_type = size_t;

public:
    /**
     * @brief       构造函数
     * @param[in]   起始值
     * @param[in]   结束值
     * @param[in]   步长
     * @ps          会抛出异常
     */
    RangeImpl(value_type begin, value_type end, value_type step = 1)
        : begin_(begin)
        , end_(end)
        , step_(step)
        , max_count_(getAdjustedCount())
    {}

    /**
     * @brief   获取长度(begin 到 end)
     */
    size_type size() const
    {
        return max_count_;
    }

    /**
     * @brief   开始位置
     */
    const_iterator begin()
    {
        return { 0, begin_, step_ };
    }

    /**
     * @brief   结束位置
     */
    const_iterator end()
    {
        return { max_count_, end_, step_ };
    }

private:
    /**
     * @brief   获取 begin 到 end 之间的长度
     */
    size_type getAdjustedCount() const
    {
        // 检测参数
        if (step_ > 0 && begin_ >= end_) {
            throw std::logic_error("end must greater than begin.");
        } else if (step_ < 0 && begin_ <= end_) {
            throw std::logic_error("end must less than begin.");
        }

        // 计算 begin 到 end 之间的长度
        size_type count = static_cast<size_type>((end_ - begin_) / step_);
        if (begin_ + (step_ * count) != end_) {
            ++count;
        }
        return count;
    }

private:
    /// 迭代器
    template<typename U>
    class Iterator
    {
    public:
        using value_type = U;
        using size_type = size_t;

    public:
        /**
         * @brief       构造函数
         * @param[in]   数量
         * @param[in]   起始值
         * @param[in]   步长
         */
        Iterator(size_type start, value_type value, value_type step)
            : cursor_(start)
            , value_(value)
            , step_(step)
        {}

        value_type operator*() const
        {
            return value_;
        }

        bool operator==(const Iterator& rhs) const
        {
            return cursor_ == rhs.cursor_;
        }

        bool operator!=(const Iterator& rhs) const
        {
            return cursor_ != rhs.cursor_;
        }

        Iterator& operator++()
        {
            value_ += step_;
            ++cursor_;
            return *this;
        }

        Iterator& operator--()
        {
            value_ -= step_;
            --cursor_;
            return *this;
        }

    private:
        //! 数量(游标)
        size_type cursor_;
        //! 步长
        const value_type step_;
        //! 值
        value_type value_;
    };

private:
    //! 起始值
    const value_type begin_;
    //! 结束值
    const value_type end_;
    //! 步长
    const value_type step_;
    //! 数量
    const size_type max_count_;
};

/**
 * @brief   构造 RangeImpl  [0, end), 步长: 1
 */
template <typename T>
RangeImpl<T> range(T end)
{
    return { {}, end, 1 };
}

/**
 * @brief   构造 RangeImpl  [begin, end), 步长: 1
 */
template <typename T>
RangeImpl<T> range(T begin, T end)
{
    return { begin, end, 1 };
}

/**
 * @brief   构造 RangeImpl  [begin, end), 步长: step
 */
template <typename T, typename U>
auto range(T begin, T end, U step) -> RangeImpl<decltype(begin + step)>
{
    return RangeImpl<decltype(begin + step)>(begin, end, step);
}

BASE_END_NAMESPACE

#endif