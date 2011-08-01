#ifndef INTRUSIVE_PTR_HPP
#define INTRUSIVE_PTR_HPP

#include <cstddef>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

#include <cppa/util/comparable.hpp>

namespace cppa {

/**
 * @brief An intrusive, reference counting smart pointer impelementation.
 * @relates ref_counted
 */
template<typename T>
class intrusive_ptr : util::comparable<intrusive_ptr<T>, const T*>,
                      util::comparable<intrusive_ptr<T>>
{

    T* m_ptr;

    inline void set_ptr(T* raw_ptr)
    {
        m_ptr = raw_ptr;
        if (raw_ptr) raw_ptr->ref();
    }

 public:

    constexpr intrusive_ptr() : m_ptr(nullptr) { }

    intrusive_ptr(T* raw_ptr) { set_ptr(raw_ptr); }

    intrusive_ptr(const intrusive_ptr& other)
    {
        set_ptr(other.m_ptr);
    }

    intrusive_ptr(intrusive_ptr&& other) : m_ptr(nullptr)
    {
        swap(other);
    }

    template<typename Y>
    intrusive_ptr(const intrusive_ptr<Y>& other)
    {
        static_assert(std::is_convertible<Y*, T*>::value,
                      "Y* is not assignable to T*");
        set_ptr(const_cast<Y*>(other.get()));
    }

    template<typename Y>
    intrusive_ptr(intrusive_ptr<Y>&& other) : m_ptr(nullptr)
    {
        swap(other);
    }

    ~intrusive_ptr()
    {
        if (m_ptr && !m_ptr->deref())
        {
            delete m_ptr;
        }
    }

    inline T* get() { return m_ptr; }

    inline const T* get() const { return m_ptr; }

    T* take()
    {
        auto result = m_ptr;
        m_ptr = nullptr;
        return result;
    }

    inline void swap(intrusive_ptr& other)
    {
        std::swap(m_ptr, other.m_ptr);
    }

    void reset(T* new_value = nullptr)
    {
        if (m_ptr && !m_ptr->deref()) delete m_ptr;
        set_ptr(new_value);
    }

    template<typename Y>
    void reset(Y* new_value)
    {
        static_assert(std::is_convertible<Y*, T*>::value,
                      "Y* is not assignable to T*");
        reset(static_cast<T*>(new_value));
    }

    intrusive_ptr& operator=(const intrusive_ptr& other)
    {
        intrusive_ptr tmp(other);
        swap(tmp);
        return *this;
    }

    intrusive_ptr& operator=(intrusive_ptr&& other)
    {
        reset();
        swap(other);
        return *this;
    }

    template<typename Y>
    intrusive_ptr& operator=(const intrusive_ptr<Y>& other)
    {
        intrusive_ptr tmp(other);
        swap(tmp);
        return *this;
    }

    template<typename Y>
    intrusive_ptr& operator=(intrusive_ptr<Y>&& other)
    {
        static_assert(std::is_convertible<Y*, T*>::value,
                      "Y* is not assignable to T*");
        reset();
        m_ptr = other.take();
        return *this;
    }

    inline T* operator->() { return m_ptr; }

    inline T& operator*() { return *m_ptr; }

    inline const T* operator->() const { return m_ptr; }

    inline const T& operator*() const { return *m_ptr; }

    inline explicit operator bool() const { return m_ptr != nullptr; }

    inline ptrdiff_t compare(const T* ptr) const
    {
        return static_cast<ptrdiff_t>(get() - ptr);
    }

    inline ptrdiff_t compare(const intrusive_ptr& other) const
    {
        return compare(other.get());
    }

    template<class C>
    intrusive_ptr<C> downcast() const
    {
        if (m_ptr) return dynamic_cast<C*>(const_cast<T*>(m_ptr));
        return nullptr;
    }

    template<class C>
    intrusive_ptr<C> upcast() const
    {
        if (m_ptr) return static_cast<C*>(const_cast<T*>(m_ptr));
        return nullptr;
    }

};

template<typename X, typename Y>
bool operator==(const intrusive_ptr<X>& lhs, const intrusive_ptr<Y>& rhs)
{
    return lhs.get() == rhs.get();
}

template<typename X, typename Y>
inline bool operator!=(const intrusive_ptr<X>& lhs, const intrusive_ptr<Y>& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // INTRUSIVE_PTR_HPP
