#ifndef BASE_TYPE_H
#define BASE_TYPE_H

#include <base/common/base_define.h>

#ifndef NDEBUG
#include <assert.h>
#endif

#include <tuple>
#include <iterator>
#include <functional>
#include <type_traits>

// C++14
#ifdef USE_CPP11
namespace std
{
    template<typename T>
    using decay_t = typename ::std::decay<T>::type;

    template<size_t Len, size_t Align = alignof(max_align_t)>
    using aligned_storage_t = typename aligned_storage<Len, Align>::type;

    template<bool B, typename T = void>
    using enable_if_t = typename enable_if<B, T>::type;

    template<bool B, typename T, typename F>
    using conditional_t = typename std::conditional<B, T, F>::type;

    template<typename T>
    using result_of_t = typename result_of<T>::type;

    template<typename T>
    using remove_reference_t = typename remove_reference<T>::type;

    template<typename T>
    using remove_pointer_t = typename remove_pointer<T>::type;

    template<typename T>
    using remove_cv_t = typename remove_cv<T>::type;

    template<std::size_t I, typename T>
    using tuple_element_t = typename tuple_element<I, T>::type;
}
#endif

// C++17 
#if defined USE_CPP11 || defined USE_CPP14
namespace std
{
    template <typename... T>
    using void_t = void;

    template<bool B>
    using bool_constant = integral_constant<bool, B>;
}
#endif

BASE_BEGIN_NAMESPACE

////////////////////////////////////////// type_convert //////////////////////////////////////////
// Taken from google-protobuf stubs/common.h
//
// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda) and others
//
// Contains basic types and utilities used by the rest of the library.

//
// Use implicit_cast as a safe version of static_cast or const_cast
// for upcasting in the type hierarchy (i.e. casting a pointer to Foo
// to a pointer to SuperclassOfFoo or casting a pointer to Foo to
// a const pointer to Foo).
// When you use implicit_cast, the compiler checks that the cast is safe.
// Such explicit implicit_casts are necessary in surprisingly many
// situations where C++ demands an exact type match instead of an
// argument type convertable to a target type.
//
// The From type can be inferred, so the preferred syntax for using
// implicit_cast is the same as for static_cast etc.:
//
//   implicit_cast<ToType>(expr)
//
// implicit_cast would have been part of the C++ standard library,
// but the proposal was submitted too late.  It will probably make
// its way into the language in the future.

//        implicit_cast< > 
// I believe this was originally going to be in the C++ standard but 
// was left out by accident. It's even milder than static_cast.
// I use it instead of static_cast<> to emphasize that I'm not doing
// anything nasty. 
// Usage is identical to static_cast<>
// 我相信它应该留在 C++ 标准中，但是意外却没有。它甚至比 static_cast 还要好
// 我使用它以代替 static_cast 来强调“我没有做任何肮脏（不安全）的事情”
// 用法与 static_cast 一致
template<typename To, typename From>
inline To implicit_cast(From const& f)
{
    return f;
}

// When you upcast (that is, cast a pointer from type Foo to type
// SuperclassOfFoo), it's fine to use implicit_cast<>, since upcasts
// always succeed.  When you downcast (that is, cast a pointer from
// type Foo to type SubclassOfFoo), static_cast<> isn't safe, because
// how do you know the pointer is really of type SubclassOfFoo?  It
// could be a bare Foo, or of type DifferentSubclassOfFoo.  Thus,
// when you downcast, you should use this macro.  In debug mode, we
// use dynamic_cast<> to double-check the downcast is legal (we die
// if it's not).  In normal mode, we do the efficient static_cast<>
// instead.  Thus, it's important to test in debug mode to make sure
// the cast is legal!
//    This is the only place in the code we should use dynamic_cast<>.
// In particular, you SHOULDN'T be using dynamic_cast<> in order to
// do RTTI (eg code like this:
//    if (dynamic_cast<Subclass1>(foo)) HandleASubclass1Object(foo);
//    if (dynamic_cast<Subclass2>(foo)) HandleASubclass2Object(foo);
// You should design the code some other way not to need this.

template<typename To, typename From>     // use like this: down_cast<T*>(foo);
inline To down_cast(From* f)                     // so we only accept pointers
{
    // Ensures that To is a sub-type of From *.  This test is here only
    // for compile-time type checking, and has no overhead in an
    // optimized build at run-time, as it will be optimized away
    // completely.
    if (false)
    {
        implicit_cast<From*, To>(0);
    }

#if !defined(NDEBUG)
    assert(f == nullptr || dynamic_cast<To>(f) != nullptr);  // RTTI: debug mode only!
#endif
    return static_cast<To>(f);
}

//        horrible_cast< >
// This is truly evil. It completely subverts C++'s type system, allowing you
// to cast from any class to any other class. Technically, using a union
// to perform the cast is undefined behaviour (even in C). But we can see if
// it is OK by checking that the union is the same size as each of its members.
// horrible_cast<> should only be used for compiler-specific workarounds.
// Usage is identical to reinterpret_cast<>.

// This union is declared outside the horrible_cast because BCC 5.5.1
// can't inline a function with a nested class, and gives a warning.

// 它确实邪恶。它完全颠覆了 C++ 的类型系统，允许从一个类型转换到任何其他类型。
// 从技术上讲，用 union 来进行转换是不明确的行为（undefined behaviour）（即便在 c 中）。
// 但是我们可以通过测试 union 的大小是否与他的每一个成员的大小一致来判断 union 是否正确。
// horrible_cast<> 应该只工作在编译器特定的工作区域内。
// 使用方法与 reinterpret_cast<> 相同

// 该 union 在 horrible_cast 之外声明，因为 BCC 5.5.1 不能够内敛带有内嵌类的函数，
// 并给出一个警告。
template <typename To, typename From>
union horrible_union
{
    To out;
    From in;
};

template <typename To, typename From>
inline To horrible_cast(const From input)
{
    horrible_union<To, From> u;
    // Cause a compile-time error if in, out and u are not the same size.
    // If the compile fails here, it means the compiler has peculiar
    // unions which would prevent the cast from working.

    // 产生编译期错误，如果输入、输出与u的大小不一致
    // 如果编译失败，表示编译器有peculiar unions，它阻止‘转换(cast)’进行
    typedef int ERROR_CantUseHorrible_cast[sizeof(From) == sizeof(u)
        /* && sizeof(From) == sizeof(To) */ ? 1 : -1];
    u.in = input;
    return u.out;
}



////////////////////////////////////////// index_sequence //////////////////////////////////////////
#ifdef USE_HALCYON_INDEX_SEQUENCE
// C++14 有 index_sequence
// 编译时的整数序列
template<size_t... Indexes>
struct index_sequence
{};

// 创建一个编译时的整数序列 [0, ..., N-1]
template<size_t N, size_t... Indexes>
struct _make_index_sequence : _make_index_sequence<N - 1, N - 1, Indexes...>
{};

template<size_t... Indexes>
struct _make_index_sequence<0, Indexes...>
{
    using type = index_sequence<Indexes...>;
};

template<size_t N>
using make_index_sequence = typename _make_index_sequence<N>::type;

// make_reverse_index_sequence
// 创建一个编译时的整数序列 [N-1, ..., 0]
template<size_t N, size_t... Indexes>
struct _make_reverse_index_sequence : _make_reverse_index_sequence<N - 1, Indexes..., N - 1>
{};

template<size_t... Indexes>
struct _make_reverse_index_sequence<0, Indexes...>
{
    using type = index_sequence<Indexes...>;
};

template<size_t N>
using make_reverse_index_sequence = typename _make_reverse_index_sequence<N>::type;

#else
// make_reverse_index_sequence
// 创建一个编译时的整数序列 [N-1, ..., 0]
template<size_t N, size_t... Indexes>
struct _make_reverse_index_sequence : _make_reverse_index_sequence<N - 1, Indexes..., N - 1>
{};

template<size_t... Indexes>
struct _make_reverse_index_sequence<0, Indexes...>
{
    using type = std::index_sequence<Indexes...>;
};

template<size_t N>
using make_reverse_index_sequence = typename _make_reverse_index_sequence<N>::type;
#endif



////////////////////////////////////////// 函数萃取 //////////////////////////////////////////
// 获取函数的实际类型、返回类型、参数个数和参数的具体类型
template<typename T>
struct function_traits;

/// 普通函数
template<typename R, typename... Args>
struct function_traits<R(Args...)>
{
    // 参数个数
    enum
    {
        arity = sizeof...(Args)
    };
    using return_type = R;  // 返回类型
    using function_type = R(Args...);  // 函数类型
    using function_pointer = R(*)(Args...);

    // 参数类型
    template<size_t I>
    struct args
    {
        static_assert(I < arity, "index is out of range");
        using type = typename std::tuple_element<I, std::tuple<Args...>>::type;
    };

    template<size_t I>
    using arg_type = typename args<I>::type;
};

/// 函数指针
template<typename R, typename... Args>
struct function_traits<R(*)(Args...)> : function_traits<R(Args...)>
{};

/// 函数对象
template<typename Callable>
struct function_traits : function_traits<decltype(&Callable::operator())>
{};

/// member function
template<typename R, typename C, typename... Args>
struct function_traits<R(C::*)(Args...)> : function_traits<R(Args...)>
{};

/// member function const
template<typename R, typename C, typename... Args>
struct function_traits<R(C::*)(Args...) const> : function_traits<R(Args...)>
{};

/// member function volatile
template<typename R, typename C, typename... Args>
struct function_traits<R(C::*)(Args...) volatile> : function_traits<R(Args...)>
{};

/// member function const volatile
template<typename R, typename C, typename... Args>
struct function_traits<R(C::*)(Args...) const volatile> : function_traits<R(Args...)>
{};

/// std::function
template<typename R, typename... Args>
struct function_traits<std::function<R(Args...)>> : function_traits<R(Args...)>
{};



////////////////////////////////////////// type //////////////////////////////////////////
template<typename T>
struct is_pointer_noref : std::is_pointer<std::remove_reference_t<T>>
{};

template<typename T>
struct is_memfunc_noref : std::is_member_function_pointer<std::remove_reference_t<T>>
{};



/////////////////////////////// 迭代器判断 ///////////////////////////////
template<typename Iter>
using iter_cate = typename std::iterator_traits<Iter>::iterator_category;

template<typename T, typename = void>
struct is_iterator : std::false_type
{};

template<typename T>
struct is_iterator<T, std::void_t<iter_cate<T>>> : std::true_type
{};



/////////////////////////////// 获取整数序列中最大值 ///////////////////////////////
template<size_t arg, size_t... rest>
struct integer_max;

template<size_t arg>
struct integer_max<arg> : std::integral_constant<size_t, arg>
{};

template<size_t arg1, size_t arg2, size_t... rest>
struct integer_max<arg1, arg2, rest...> : std::integral_constant<size_t, arg1 >= arg2
    ? integer_max<arg1, rest...>::value : integer_max<arg2, rest...>::value>
{};

BASE_END_NAMESPACE

#endif