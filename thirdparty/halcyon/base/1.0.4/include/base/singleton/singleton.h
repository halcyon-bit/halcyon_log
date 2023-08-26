#ifndef BASE_SINGLETON_H
#define BASE_SINGLETON_H

#include <base/common/noncopyable.h>

#include <mutex>
#include <cstdlib>
#include <cassert>

BASE_BEGIN_NAMESPACE

namespace detail
{
    // 在编译期间判断泛型T中是否存在 no_destroy 方法
    // 当写入 detail::has_no_destroy<T>::value 这段代码的时候，会触发 sizeof(test<T>(0)) 的计算。
    // 首先匹配模板 template<typename C> static char test(decltype(&C::no_destroy));
    //    如果匹配成功，那么 test 函数的返回类型也就为 char，
    // 否则根据 SFINAE 原理，test 函数的返回类型就是 int32_t。
    //（sizeof 关键字能够在编译期确定类型大小，所以无需提供函数体也能得到返回值大小。）
    // 再通过判断 test 返回值类型的大小，就可以推断出是否存在 has_no_destroy。
    // This doesn't detect inherited member functions!
    // http://stackoverflow.com/questions/1966362/sfinae-to-check-for-inherited-member-functions
    template<typename T>
    struct has_no_destroy
    {
        template <typename C>
        static char test(decltype(&C::no_destroy));

        template <typename C>
        static int32_t test(...);

        const static bool value = sizeof(test<T>(0)) == 1;
    };
}

/// 单例
template<typename T>
class Singleton : noncopyable
{
public:
    Singleton() = delete;
    ~Singleton() = delete;

    static T& instance()
    {
        std::call_once(flag_, &Singleton::init);
        assert(value_ != nullptr);
        return *value_;
    }

private:
    static void init()
    {
        value_ = new T();
        if (!detail::has_no_destroy<T>::value) {
            ::atexit(destory);
        }
    }

    static void destory()
    {
        // 在 C++ 中，类型有 Complete type 和 Incomplete type 之分，
        // 对于 Complete type, 它的大小在编译时是可以确定的，
        // 而对于 Incomplete type, 它的大小在编译时是不能确定的。
        // 当T为不完整类型时，sizeof(T) 给出的是0，将产生编译期错误
        typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
        T_must_be_complete_type dummy; (void)dummy;

        // 用 delete 删除一个只有声明但无定义的类型的指针(即不完整类型)是危险的
        delete value_;
        value_ = nullptr;
    }

private:
    static T* value_;
    static std::once_flag flag_;
};

template<typename T>
std::once_flag Singleton<T>::flag_;

template<typename T>
T* Singleton<T>::value_ = nullptr;

BASE_END_NAMESPACE

#endif