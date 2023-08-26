#ifndef BASE_DEFER_H
#define BASE_DEFER_H

#include <base/common/noncopyable.h>

#include <functional>

/**
 * @brief   defer 语句定义一个延迟执行闭包函数的对象
 */

 // 注意事项:
 // 1. defer 定义的对象在超出作用域时执行闭包函数(析构函数)
 // 2. defer 定义的对象在同一个文件内部标识符不同(根据行号生成)
 // 3. defer 在全局作用域使用可能会出现重名现象(行号相同)
 // 4. defer 在判断语句使用可能提前执行(作用域结束时)
 // 5. defer 在循环语句内使用无效(作用域结束时)

#define defer _DEFER_ACTION_MAKE

// auto _defer_action_line_??? = _DeferredActionCtor([&](){ ... })
#define _DEFER_ACTION_MAKE auto \
    _DEFER_ACTION_VAR(_defer_action_line_, __LINE__) = halcyon::base::_DeferredActionCtor

#define _DEFER_ACTION_VAR(a, b) _DEFER_TOKEN_CONNECT(a, b)
#define _DEFER_TOKEN_CONNECT(a, b) a ## b

BASE_BEGIN_NAMESPACE

/// 持有闭包函数
class _DeferredAction final : noncopyable
{
public:
    /**
     * @brief   移动构造函数
     */
    _DeferredAction(_DeferredAction&& that) noexcept
        : func_(std::forward<std::function<void()>>(that.func_))
    {
        that.func_ = nullptr;
    }

    /**
     * @brief   析构函数(执行闭包函数)
     */
    ~_DeferredAction()
    {
        if (nullptr != func_) {
            func_();
        }
    }

private:
    template<typename T>
    friend _DeferredAction _DeferredActionCtor(T&& p);  // 创建 _DeferredAction 对象的友元函数

    /**
     * @brief   构造函数(私有, 外部无法创建对象)
     */
    template<typename T>
    _DeferredAction(T&& p)
        : func_(std::bind(std::forward<T>(p)))
    {}

    _DeferredAction() = delete;

private:
    //! 需要执行的闭包函数
    std::function<void()> func_;
};

/**
 * @brief   创建 _DeferredAction 对象
 */
template<typename T>
_DeferredAction _DeferredActionCtor(T&& p)
{
    return _DeferredAction(std::forward<T>(p));
}

BASE_END_NAMESPACE

#endif