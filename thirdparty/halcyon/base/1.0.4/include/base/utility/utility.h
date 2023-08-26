#ifndef BASE_UTILITY_H
#define BASE_UTILITY_H

#include <base/utility/type.h>

#include <utility>
#include <stdexcept>

#ifdef USE_HALCYON_INDEX_SEQUENCE
#define HALCYON_INDEX_NS   base::
#else
#define HALCYON_INDEX_NS   std::
#endif

#ifdef USE_CPP11
// C++11 以上会有的功能
namespace std
{
    // C++14
    template<typename T, typename Other = T>
    inline T exchange(T& val, Other&& new_val)
    {
        T old_val = ::std::move(val);
        val = ::std::forward<Other>(new_val);
        return old_val;
    }
}
#endif  // USE_CPP11


BASE_BEGIN_NAMESPACE
////////////////////////////////////////// genHashFunction //////////////////////////////////////////
/* MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 * 2. sizeof(int) == 4
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
static inline unsigned int genHashFunction(const void* key, int len)
{
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    uint32_t seed = 5381;
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    uint32_t h = seed ^ len;

    /* Mix 4 bytes at a time into the hash */
    const unsigned char* data = (const unsigned char*)key;

    while (len >= 4) {
        uint32_t k = *(uint32_t*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch (len) {
    case 3:
        h ^= data[2] << 16;
    case 2: 
        h ^= data[1] << 8;
    case 1: 
        h ^= data[0]; h *= m;
    };

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (unsigned int)h;
}

////////////////////////////////////////// invoke //////////////////////////////////////////
// C++17 有 invoke
#ifdef USE_HALCYON_INVOKE_APPLY
template<typename F, typename... Args>
inline std::enable_if_t<is_pointer_noref<F>::value, typename function_traits<std::decay_t<F>>::return_type> invoke(F&& f, Args&&... args)
{
    return (*std::forward<F>(f))(std::forward<Args>(args)...);
}

template<typename F, typename... Args>
inline std::enable_if_t<!is_pointer_noref<F>::value && !is_memfunc_noref<F>::value, typename function_traits<std::decay_t<F>>::return_type> invoke(F&& f, Args&&... args)
{
    return std::forward<F>(f)(std::forward<Args>(args)...);
}

template<typename F, typename C, typename... Args>
inline std::enable_if_t<is_memfunc_noref<F>::value&& is_pointer_noref<C>::value, typename function_traits<std::decay_t<F>>::return_type> invoke(F&& f, C&& obj, Args&&... args)
{
    return (std::forward<C>(obj)->*std::forward<F>(f))(std::forward<Args>(args)...);
}

template<typename F, typename C, typename... Args>
inline std::enable_if_t<is_memfunc_noref<F>::value && !is_pointer_noref<C>::value, typename function_traits<std::decay_t<F>>::return_type> invoke(F&& f, C&& obj, Args&&... args)
{
    return (std::forward<C>(obj).*std::forward<F>(f))(std::forward<Args>(args)...);
}
#endif  // USE_HALCYON_INVOKE_APPLY


////////////////////////////////////////// tuple helper //////////////////////////////////////////

////////////////////////////////////////// std::tuple 根据元素值获取索引位置
// find_tuple_index
namespace detail
{
    template<size_t N, typename Tuple, typename T>
    static std::enable_if_t<std::is_same<std::tuple_element_t<N, Tuple>, T>::value, bool>
        compare(const Tuple& t, const T& v)
    {
        return std::get<N>(t) == v;
    }
    template<size_t N, typename Tuple, typename T>
    static std::enable_if_t<!std::is_same<std::tuple_element_t<N, Tuple>, T>::value, bool>
        compare(const Tuple& t, const T& v)
    {
        return false;
    }

    template<size_t I, typename T, typename... Args>
    struct find_tuple_index_helper
    {
        static int find(const std::tuple<Args...>& t, T&& val)
        {
            using U = std::remove_reference_t<std::remove_cv_t<T>>;
            using V = std::tuple_element_t<I - 1, std::tuple<Args...>>;
            return (std::is_same<U, V>::value && compare<I - 1>(t, val)) ? I - 1
                : find_tuple_index_helper<I - 1, T, Args...>::find(t, std::forward<T>(val));
        }
    };

    template<typename T, typename... Args>
    struct find_tuple_index_helper<0, T, Args...>
    {
        static int find(const std::tuple<Args...>& t, T&& val)
        {
            using U = std::remove_reference_t<std::remove_cv_t<T>>;
            using V = std::tuple_element_t<0, std::tuple<Args...>>;
            return (std::is_convertible<U, V>::value && compare<0>(t, val)) ? 0 : -1;
        }
    };
}

/**
 * @brief       获取 val 在 tuple 中的索引位置
 * @param[in]   tuple
 * @param[in]   value, 必要时需要显示指明类型
 * @return      索引位置，没找到返回 -1
 */
template<typename T, typename... Args>
constexpr int find_tuple_index(const std::tuple<Args...>& t, T&& val)
{
    return detail::find_tuple_index_helper<sizeof...(Args), T, Args...>::find(t, std::forward<T>(val));
}


////////////////////////////////////////// 遍历 std::tuple
// tuple_for_each
namespace detail
{
    template<typename F, size_t... Indexes, typename Tuple>
    constexpr void for_each_helper(F&& f, HALCYON_INDEX_NS index_sequence<Indexes...>, Tuple&& t)
    {
#if defined USE_CPP11 || defined USE_CPP14
        int _[] = { (f(std::get<Indexes>(std::forward<Tuple>(t))), 0)... };  // 逗号运算符，解包
#else
        (f(std::get<Indexes>(std::forward<Tuple>(t))), ...);  // C++17 折叠表达式
#endif  // USE_CPP11 || USE_CPP14
    }
}

/**
 * @brief       遍历 tuple
 * @param[in]   函数，用于处理 tuple 内的每一个值，仅接受一个参数
 * @param[in]   tuple
 */
template<typename F, typename Tuple>
constexpr std::enable_if_t<std::tuple_size<std::remove_reference_t<Tuple>>::value != 0>
tuple_for_each(F&& f, Tuple&& t)
{
    detail::for_each_helper(std::forward<F>(f),
        HALCYON_INDEX_NS make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>(),
        t);
}
template<typename F, typename Tuple>
constexpr std::enable_if_t<std::tuple_size<std::remove_reference_t<Tuple>>::value == 0>
tuple_for_each(F&& f, Tuple&& t)
{}


////////////////////////////////////////// 反转 std::tuple
// reverse_tuple
namespace detail
{
    template<typename... Args, size_t... Indexes>
#ifdef USE_CPP11
    auto reverse_tuple_impl(const std::tuple<Args...>& t, base::index_sequence<Indexes...>) -> decltype(std::make_tuple(std::get<Indexes>((t))...))
#else
    decltype(auto) reverse_tuple_impl(const std::tuple<Args...>& t, HALCYON_INDEX_NS index_sequence<Indexes...>)
#endif  // USE_CPP11
    {
        return std::make_tuple(std::get<Indexes>((t))...);
    }
}

/**
 * @brief       反转 tuple
 * @param[in]   原始 tuple
 * @return      反转后的 tuple
 */
template<typename... Args>
#ifdef USE_CPP11
auto reverse_tuple(const std::tuple<Args...>& t) -> decltype(detail::reverse_tuple_impl((t), make_reverse_index_sequence<sizeof...(Args)>()))
#else
decltype(auto) reverse_tuple(const std::tuple<Args...>& t)
#endif  // USE_CPP11
{
    return detail::reverse_tuple_impl(t, make_reverse_index_sequence<sizeof...(Args)>());
}


/////////////////////////////// std::tuple 应用于函数
// C++17 有 apply
#ifdef USE_HALCYON_INVOKE_APPLY

namespace detail
{
    template<typename F, typename Tuple, size_t... Indexes>
#ifdef USE_CPP11
    typename function_traits<std::decay_t<F>>::return_type apply_helper(F&& f, Tuple&& t, base::index_sequence<Indexes...>)
#else
    decltype(auto) apply_helper(F&& f, Tuple&& t, HALCYON_INDEX_NS index_sequence<Indexes...>)
#endif  // USE_CPP11
    {
        return base::invoke(std::forward<F>(f), std::get<Indexes>(std::forward<Tuple>(t))...);
    }
}

/**
 * @brief       应用于函数，将 tuple 中的元素作为函数参数，进行调用
 * @param[in]   函数
 * @param[in]   tuple
 * @return      函数结果
 */
template<typename F, typename Tuple>
#ifdef USE_CPP11
typename function_traits<std::decay_t<F>>::return_type apply(F&& f, Tuple&& t)
#else
decltype(auto) apply(F&& f, Tuple&& t)
#endif  // USE_CPP11
{
    return detail::apply_helper(std::forward<F>(f), std::forward<Tuple>(t),
        HALCYON_INDEX_NS make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>());
}

#endif  // USE_HALCYON_INVOKE_APPLY


/////////////////////////////// 合并 tuple
namespace detail
{
    template<size_t N, typename Tuple1, typename Tuple2>
    using pair_type = std::pair<std::tuple_element_t<N, Tuple1>, std::tuple_element_t<N, Tuple2>>;

    template<size_t N, typename Tuple1, typename Tuple2>
    pair_type<N, Tuple1, Tuple2> pair(const Tuple1& t1, const Tuple2& t2)
    {
        return std::make_pair(std::get<N>(t1), std::get<N>(t2));
    }

    template<size_t... Indexes, typename Tuple1, typename Tuple2>
#ifdef USE_CPP11
    auto pairs_helper(base::index_sequence<Indexes...>, const Tuple1& t1, const Tuple2& t2) -> decltype(std::make_tuple(detail::pair<Indexes>(t1, t2)...))
#else
    decltype(auto) pairs_helper(HALCYON_INDEX_NS index_sequence<Indexes...>, const Tuple1& t1, const Tuple2& t2)
#endif  // USE_CPP11
    {
        return std::make_tuple(detail::pair<Indexes>(t1, t2)...);
    }
}

template<typename... Args1, typename... Args2>
#ifdef USE_CPP11
auto zip(const std::tuple<Args1...>& t1, const std::tuple<Args2...>& t2) -> decltype(detail::pairs_helper(base::make_index_sequence<sizeof...(Args1)>(), t1, t2))
#else
decltype(auto) zip(const std::tuple<Args1...>& t1, const std::tuple<Args2...>& t2)
#endif  // USE_CPP11
{
    static_assert(sizeof...(Args1) == sizeof...(Args2), "tuples should be the same size.");
    return detail::pairs_helper(HALCYON_INDEX_NS make_index_sequence<sizeof...(Args1)>(), t1, t2);
}

BASE_END_NAMESPACE


#ifdef _BASE_TEST_

#include <string>
#include <memory>

BASE_BEGIN_NAMESPACE

template<typename T>
std::string typeName()
{
    using type = typename std::remove_reference<T>::type;
    std::unique_ptr<char, void(*)(void*)> own(nullptr, std::free);
    std::string res = own != nullptr ? own.get() : typeid(type).name();
    if (std::is_const<type>::value)
        res += " const";
    if (std::is_volatile<type>::value)
        res += " volatile";
    if (std::is_lvalue_reference<T>::value)
        res += "&";
    else if (std::is_rvalue_reference<T>::value)
        res += "&&";
    return res;
}

BASE_END_NAMESPACE

#endif  // _BASE_TEST_

#endif  // BASE_UTILITY_H