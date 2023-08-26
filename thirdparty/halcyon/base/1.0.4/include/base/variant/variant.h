#ifndef BASE_VARIANT_H
#define BASE_VARIANT_H

#include <base/utility/utility.h>

#include <typeindex>

BASE_BEGIN_NAMESPACE

namespace detail
{
    /// 内存对齐大小(最大的 align)
    template<typename... T>
    struct max_align : std::integral_constant<int, integer_max<std::alignment_of<T>::value...>::value>
    {};


    /// 是否包含某种类型(Rest 中是否有 T)(可以转换也行(非基本类型))
    template<typename T, typename... Rest>
    struct is_contains;

    template<typename T, typename U, typename... Rest>
    struct is_contains<T, U, Rest...>
        : std::conditional<std::is_same<T, U>::value  // 相同类型
            || ((!std::is_arithmetic<T>::value && !std::is_arithmetic<U>::value) && std::is_constructible<U, T>::value), std::true_type, is_contains<T, Rest...>>::type
    {
        using type = typename std::conditional<std::is_same<T, U>::value
            || ((!std::is_arithmetic<T>::value && !std::is_arithmetic<U>::value) && std::is_constructible<U, T>::value), U, typename is_contains<T, Rest...>::type>::type;
    };

    template<typename T>
    struct is_contains<T> : std::false_type
    {
        using type = void;
    };


    /// 通过类型获取索引
    template<typename T, typename... Rest>
    struct index_of;

    template<typename T, typename U, typename... Rest>
    struct index_of<T, U, Rest...>
        : std::integral_constant<int32_t, index_of<T, Rest...>::value >= 0 ? index_of<T, Rest...>::value + 1 : index_of<T, Rest...>::value>
    {};

    template<typename T, typename... Rest>
    struct index_of<T, T, Rest...> : std::integral_constant<int32_t, 0>
    {};

    template<typename T>
    struct index_of<T> : std::integral_constant<int32_t, -1>
    {};


    /// 获取指定位置的类型
    template<size_t index, typename... T>
    struct at;

    template<size_t index, typename T, typename... Rest>
    struct at<index, T, Rest...>
    {
        using type = typename at<index - 1, Rest...>::type;
    };

    template<typename T, typename... Rest>
    struct at<0, T, Rest...>
    {
        using type = T;
    };
}

/**
 * @brief   类似于 union，可以定义多种类型，允许赋不同类型的值。
 *
 * @ps      C++17 std::variant
 */
template<typename... T>
class Variant
{
    static_assert(sizeof...(T) > 0, "no template arguments!");

    static const size_t kDataSize = integer_max<sizeof(T)...>::value;
    static const size_t kAlignSize = detail::max_align<T...>::value;
    using value_type = std::aligned_storage_t<kDataSize, kAlignSize>;

public:
    /// 获取 T... 中某位置的类型
    template<size_t index>
    using index_type = typename detail::at<index, T...>::type;

public:
    Variant()
        : type_(std::type_index(typeid(void)))
    {}

    Variant(const Variant& rhs)
        : type_(rhs.type_)
    {
        copy(rhs.type_, &rhs.data_, &data_);
    }

    Variant(Variant&& rhs) noexcept
        : type_(rhs.type_)
    {
        move(rhs.type_, &rhs.data_, &data_);
        rhs.destroy(rhs.type_, &rhs.data_);
        type_ = std::exchange(rhs.type_, std::type_index(typeid(void)));
    }

    Variant& operator=(const Variant& rhs)
    {
        if (&rhs != this) {
            destroy(type_, &data_);
            copy(rhs.type_, &rhs.data_, &data_);
            type_ = rhs.type_;
        }
        return *this;
    }

    Variant& operator=(Variant&& rhs) noexcept
    {
        if (&rhs != this) {
            destroy(type_, &data_);
            move(rhs.type_, &rhs.data_, &data_);
            rhs.destroy(rhs.type_, &rhs.data_);
            type_ = std::exchange(rhs.type_, std::type_index(typeid(void)));
        }
        return *this;
    }

    ~Variant()
    {
        destroy(type_, &data_);
    }

public:
    template<typename U, typename = std::enable_if_t<detail::is_contains<std::decay_t<U>, T...>::value>>
    Variant(U&& value)
        : type_(std::type_index(typeid(void)))
    {
        using type = typename detail::is_contains<std::decay_t<U>, T...>::type;
        new(&data_) type(std::forward<U>(value));
        type_ = std::type_index(typeid(type));
    }

    template<typename U, typename = std::enable_if_t<detail::is_contains<std::decay_t<U>, T...>::value>>
    Variant& operator=(U&& value)
    {
        destroy(type_, &data_);
        using type = typename detail::is_contains<std::decay_t<U>, T...>::type;
        new(&data_) type(std::forward<U>(value));
        type_ = std::type_index(typeid(type));
        return *this;
    }

public:
    /**
     * @brief   是否为某种类型
     */
    template<typename U>
    bool is() const
    {
        return (type_ == std::type_index(typeid(U)));
    }

    bool empty() const
    {
        return (type_ == std::type_index(typeid(void)));
    }

    /**
     * @brief   获取当前存储数据的类型
     */
    std::type_index type() const
    {
        return type_;
    }

    /**
     * @brief   获取某种类型在 Variant 类型包中的索引
     *        索引从 0 开始
     */
    template<typename U>
    int32_t indexOf()
    {
        return detail::index_of<U, T...>::value;
    }

    /**
     * @brief   获取 Variant 中存储的数据
     * @ps      类型不存在，则抛异常
     */
    template<typename U>
    std::decay_t<U>& get()
    {
        using type = std::decay_t<U>;
        if (!is<type>()) {
            throw std::bad_cast();
        }
        return *(type*)(&data_);
    }

public:
    bool operator==(const Variant& rhs) const
    {
        return type_ == rhs.type_;
    }

private:
    void destroy(const std::type_index& type, void* data)
    {
        // 解包
#if defined USE_CPP11 || defined USE_CPP14
        int _[] = { (destroyImpl<T>(type, data), 0)... };  // 逗号运算符，解包
        //[](int&&...) {}((destroyImpl<T>(type, data), 0)...);
#else
        ((destroyImpl<T>(type, data)), ...);  // C++17 折叠表达式
#endif
    }

    template<typename U>
    void destroyImpl(const std::type_index& type, void* data)
    {
        if (type == std::type_index(typeid(U))) {
            reinterpret_cast<U*>(data)->~U();
        }
    }

    void move(const std::type_index& o_type, void* o_data, void* n_data)
    {
#if defined USE_CPP11 || defined USE_CPP14
        int _[] = { (moveImpl<T>(o_type, o_data, n_data), 0)... };
        //[](int&&...) {}((moveImpl<T>(o_type, o_data, n_data), 0)...);
#else
        ((moveImpl<T>(o_type, o_data, n_data)), ...);
#endif
    }

    template<typename U>
    void moveImpl(const std::type_index& o_type, void* o_data, void* n_data)
    {
        if (o_type == std::type_index(typeid(U))) {
            new (n_data) U(std::move(*reinterpret_cast<U*>(o_data)));
        }
    }

    void copy(const std::type_index& o_type, const void* o_data, void* n_data)
    {
#if defined USE_CPP11 || defined USE_CPP14
        int _[] = { (copyImpl<T>(o_type, o_data, n_data), 0)... };
        //[](int&&...) {}((copyImpl<T>(o_type, o_data, n_data), 0)...);
#else
        ((copyImpl<T>(o_type, o_data, n_data)), ...);
#endif
    }

    template<typename U>
    void copyImpl(const std::type_index& o_type, const void* o_data, void* n_data)
    {
        if (o_type == std::type_index(typeid(U))) {
            new (n_data) U(*reinterpret_cast<const U*>(o_data));
        }
    }

private:
    value_type data_;
    std::type_index type_;
};

BASE_END_NAMESPACE

#endif