#ifndef BASE_WEAK_CALLBACK_H
#define BASE_WEAK_CALLBACK_H

#include <base/common/base_define.h>

#include <memory>
#include <functional>

BASE_BEGIN_NAMESPACE

/// 弱回调(C: 类, 针对类的成员函数)
template<typename C, typename... Args>
class WeakCallback
{
public:
    /**
     * @brief       构造函数
     * @param[in]   对象的弱指针
     * @param[in]   函数指针
     */
    WeakCallback(const std::weak_ptr<C>& object,
        const std::function<void(C*, Args...)>& func)
        : object_(object)
        , function_(func)
    {}

    /**
     * @brief       重载操作符()，调用函数
     * @param[in]   参数
     */
    void operator()(Args&&... args) const
    {
        std::shared_ptr<C> ptr(object_.lock());
        if (ptr) {
            // 若指针有效，则调用相应函数
            function_(ptr.get(), std::forward<Args>(args)...);
        }
    }

private:
    //! 类对象
    std::weak_ptr<C> object_;
    //! 类的成员函数
    std::function<void(C*, Args...)> function_;
};

/**
 * @brief   创建弱回调对象
 */
template<typename C, typename... Args>
WeakCallback<C, Args...> makeWeakCallback(const std::shared_ptr<C>& object,
    void (C::*func)(Args...))
{
    return WeakCallback<C, Args...>(object, func);
}

template<typename C, typename... Args>
WeakCallback<C, Args...> makeWeakCallback(const std::shared_ptr<C>& object,
    void (C::*func)(Args...) const)
{
    return WeakCallback<C, Args...>(object, func);
}

BASE_END_NAMESPACE

#endif