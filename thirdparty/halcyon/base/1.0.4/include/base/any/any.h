#ifndef BASE_ANY_H
#define BASE_ANY_H

#include <base/utility/type.h>  // std::decay_t, std::enable_if_t

#include <memory>
#include <typeindex>

BASE_BEGIN_NAMESPACE

/**
 * @brief   一个特殊的只能容纳一个元素的容器，它可以擦除类型，可以赋给它任何
 *      类型的值，不过在使用的时候需要根据实际类型将 any 对象转换为实际的对象。
 *
 *      通过继承去擦除类型，基类是不含模板参数的，派生类中才有模板参数，这个模板
 *  参数类型正是赋值的类型。
 *
 * @ps      C++17 中已有 std::any 类型
 */
class Any final
{
public:
    Any()
        : type_(std::type_index(typeid(void)))
    {}

    /**
     * @brief   拷贝构造函数
     */
    Any(const Any& that)
        : ptr_(that.clone())
        , type_(that.type_)
    {}

    /**
     * @brief   移动构造函数
     */
    Any(Any&& that) noexcept
        : ptr_(std::move(that.ptr_))
        , type_(that.type_)
    {
        that.type_ = std::type_index(typeid(void));
    }

    /**
     * @brief   赋值操作符重载
     */
    Any& operator=(const Any& rhs)
    {
        if (this != &rhs) {
            ptr_ = rhs.clone();
            type_ = rhs.type_;
        }
        return *this;
    }
    Any& operator=(Any&& rhs) noexcept
    {
        if (this != &rhs) {
            ptr_ = std::move(rhs.ptr_);
            type_ = rhs.type_;
            rhs.type_ = std::type_index(typeid(void));
        }
        return *this;
    }

public:
    /**
     * @brief   其他类型转换为 Any(排除 Any 类型 std::enable_if)
     */
    template<typename U, typename = std::enable_if_t<!std::is_same<std::decay_t<U>, Any>::value, U>>
    Any(U&& value)
        : ptr_(new Derived<std::decay_t<U>>(std::forward<U>(value)))
        , type_(std::type_index(typeid(std::decay_t<U>)))
    {}

    template<typename U, typename = std::enable_if_t<!std::is_same<std::decay_t<U>, Any>::value, U>>
    Any& operator=(U&& value)
    {
        ptr_.reset(new Derived<std::decay_t<U>>(std::forward<U>(value)));
        type_ = std::type_index(typeid(std::decay_t<U>));
        return *this;
    }

public:
    /**
     * @brief   Any 是否为空
     */
    bool isNull() const
    {
        return !bool(ptr_);
    }

    /**
     * @brief   是否为某种类型
     */
    template<typename U>
    bool is() const
    {
        return type_ == std::type_index(typeid(U));
    }

    /**
     * @brief   转换为实际类型(不符合则抛出异常)
     */
    template<typename U>
    U& anyCast()
    {
        if (!is<U>()) {
            // 类型不符
            throw std::bad_cast();
        }
        auto derived = static_cast<Derived<U>*>(ptr_.get());
        return derived->value_;
    }

private:
    struct Base;
    using BasePtr = std::unique_ptr<Base>;
    //using BasePtr = std::shared_ptr<Base>;

    struct Base
    {
        virtual ~Base() {};
        virtual BasePtr clone() const = 0;
    };

    template<typename T>
    struct Derived : Base
    {
        template<typename U>
        Derived(U&& value)
            : value_(std::forward<U>(value))
        {}

        BasePtr clone() const override
        {
            return BasePtr(new Derived<T>(value_));
        }

        T value_;
    };

private:
    /**
     * @brief   创建数据
     */
    BasePtr clone() const
    {
        if (ptr_ != nullptr) {
            return ptr_->clone();
        } else {
            return nullptr;
        }
    }

private:
    //! 值
    BasePtr ptr_;
    //! 类型
    std::type_index type_;
};

BASE_END_NAMESPACE

#endif