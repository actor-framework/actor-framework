#ifndef COW_PTR_HPP
#define COW_PTR_HPP

#include <stdexcept>
#include <type_traits>

#include "cppa/intrusive_ptr.hpp"

#include "cppa/util/is_copyable.hpp"
#include "cppa/util/has_copy_member_fun.hpp"

namespace cppa { namespace detail {

template<typename T>
constexpr int copy_method()
{
    return util::has_copy_member_fun<T>::value
            ? 2
            : (util::is_copyable<T>::value ? 1 : 0);
}

// is_copyable
template<typename T>
T* copy_of(const T* what, std::integral_constant<int, 1>)
{
    return new T(*what);
}

// has_copy_member_fun
template<typename T>
T* copy_of(const T* what, std::integral_constant<int, 2>)
{
    return what->copy();
}

} } // namespace cppa::detail

namespace cppa {

template<typename T>
class cow_ptr
{

    static_assert(detail::copy_method<T>() != 0, "T is not copyable");

    typedef std::integral_constant<int, detail::copy_method<T>()> copy_of_token;

    intrusive_ptr<T> m_ptr;

    T* detach_ptr()
    {
        T* ptr = m_ptr.get();
        if (!ptr->unique())
        {
            T* new_ptr = detail::copy_of(ptr, copy_of_token());
            cow_ptr tmp(new_ptr);
            swap(tmp);
            return new_ptr;
        }
        return ptr;
    }

 public:

    template<typename Y>
    cow_ptr(Y* raw_ptr) : m_ptr(raw_ptr) { }

    cow_ptr(T* raw_ptr) : m_ptr(raw_ptr) { }

    cow_ptr(const cow_ptr& other) : m_ptr(other.m_ptr) { }

    template<typename Y>
    cow_ptr(const cow_ptr<Y>& other) : m_ptr(const_cast<Y*>(other.get())) { }

    inline void swap(cow_ptr& other)
    {
        m_ptr.swap(other.m_ptr);
    }

    cow_ptr& operator=(const cow_ptr& other)
    {
        cow_ptr tmp(other);
        swap(tmp);
        return *this;
    }

    template<typename Y>
    cow_ptr& operator=(const cow_ptr<Y>& other)
    {
        cow_ptr tmp(other);
        swap(tmp);
        return *this;
    }

    inline T* get() { return detach_ptr(); }

    inline const T* get() const { return m_ptr.get(); }

    inline T* operator->() { return detach_ptr(); }

    inline T& operator*() { return detach_ptr(); }

    inline const T* operator->() const { return m_ptr.get(); }

    inline const T& operator*() const { return *m_ptr.get(); }

    inline explicit operator bool() const { return static_cast<bool>(m_ptr); }

};

} // namespace cppa

#endif // COW_PTR_HPP
