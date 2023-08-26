#ifndef BASE_OPTIONAL_H
#define BASE_OPTIONAL_H

#include <base/utility/type.h>  // std::aligned_storage_t

#include <stdexcept>

BASE_BEGIN_NAMESPACE

/**
 * @brief   Optional 管理一个值，既可以存在也可以不存在的值，只有当 Optional
 *      被初始化之后，这个 Optional 才是有效的。
 *
 * @ps      C++17 中已有 std::optional 类型
 */
template<typename T>
class Optional
{
    using value_type = std::aligned_storage_t<sizeof(T), std::alignment_of<T>::value>;
public:  // 构造函数
    Optional()
    {}

    Optional(const T& val)
    {
        create(val);
    }

    Optional(T&& val)
    {
        create(std::forward<T>(val));
    }

    Optional(const Optional& rhs)
    {
        if (rhs.isInit()) {
            copy(rhs.data_);
        }
    }

    Optional(Optional&& rhs) noexcept
    {
        if (rhs.isInit()) {
            move(std::move(rhs.data_));
            rhs.destroy();
        }
    }

    ~Optional()
    {
        destroy();
    }

    // 赋值操作符
    Optional& operator=(const Optional& rhs)
    {
        if (this == &rhs) {
            return *this;
        }
        destroy();  // 销毁自身
        if (rhs.isInit()) {
            copy(rhs.data_);
        }
        return *this;
    }

    Optional& operator=(Optional&& rhs) noexcept
    {
        if (this == &rhs) {
            return *this;
        }
        destroy();  // 销毁自身
        if (rhs.isInit()) {
            move(std::move(rhs.data_));
            rhs.destroy();
        }
        return *this;
    }

    Optional& operator=(const T& val)
    {
        destroy();  // 销毁自身
        create(val);
        return *this;
    }
    Optional& operator=(T&& val)
    {
        destroy();  // 销毁自身
        create(std::forward<T>(val));
        return *this;
    }

public:
    /**
     * @brief   是否初始化
     */
    bool isInit() const
    {
        return init_;
    }

    /**
     * @brief   构造
     */
    template<typename... Args>
    void emplace(Args&&... args)
    {
        destroy();
        create(std::forward<Args>(args)...);
    }

public:
    /**
     * @brief   获取数据，若未初始化，则抛出异常
     */
    T& operator*()
    {
        if (isInit()) {
            return *((T*)(&data_));
        } else {
            throw std::logic_error("Optional not initialized");
        }
    }

    const T& operator*() const
    {
        if (isInit()) {
            return *((T*)(&data_));
        } else {
            throw std::logic_error("Optional not initialized");
        }
    }

    T* operator->()
    {
        return &operator*();
    }
    const T* operator->() const
    {
        return &operator*();
    }

public:
    /**
     * @brief   是否初始化
     */
    operator bool() const
    {
        return init_;
    }

private:
    template<typename... Args>
    void create(Args&&... args)
    {
        new (&data_) T(std::forward<Args>(args)...);
        init_ = true;
    }

    void destroy()
    {
        if (init_) {
            ((T*)(&data_))->~T();
            init_ = false;
        }
    }

    void copy(const value_type& val)
    {
        new (&data_) T(*((T*)(&val)));
        init_ = true;
    }

    void move(value_type&& val)
    {
        new (&data_) T(std::move(*((T*)(&val))));
        init_ = true;
    }

private:
    //! 是否初始化
    bool init_{ false };
    //! 数据
    value_type data_;
};

BASE_END_NAMESPACE

#endif