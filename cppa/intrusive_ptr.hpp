/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_INTRUSIVE_PTR_HPP
#define CPPA_INTRUSIVE_PTR_HPP

#include <cstddef>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

#include "cppa/util/comparable.hpp"

namespace cppa {

template<class From, class To>
struct convertible {
    inline To convert() const {
        return static_cast<const From*>(this)->do_convert();
    }
};

/**
 * @brief An intrusive, reference counting smart pointer impelementation.
 * @relates ref_counted
 */
template<typename T>
class intrusive_ptr : util::comparable<intrusive_ptr<T> >,
                      util::comparable<intrusive_ptr<T>, const T*>,
                      util::comparable<intrusive_ptr<T>, std::nullptr_t> {

 public:

    typedef T*       pointer;
    typedef const T* const_pointer;
    typedef T        element_type;
    typedef T&       reference;
    typedef const T& const_reference;

    constexpr intrusive_ptr() : m_ptr(nullptr) { }

    intrusive_ptr(pointer raw_ptr) { set_ptr(raw_ptr); }

    intrusive_ptr(intrusive_ptr&& other) : m_ptr(other.release()) { }

    intrusive_ptr(const intrusive_ptr& other) { set_ptr(other.get()); }

    template<typename Y>
    intrusive_ptr(intrusive_ptr<Y> other) : m_ptr(other.release()) {
        static_assert(std::is_convertible<Y*, T*>::value,
                      "Y* is not assignable to T*");
    }

    // enables "actor_ptr s = self"
    template<typename From>
    intrusive_ptr(const convertible<From, pointer>& from) {
        set_ptr(from.convert());
    }

    ~intrusive_ptr() {
        if (m_ptr && !m_ptr->deref()) delete m_ptr;
    }

    inline void swap(intrusive_ptr& other) {
        std::swap(m_ptr, other.m_ptr);
    }

    /**
     * @brief Returns the raw pointer without modifying reference count.
     */
    pointer release() {
        auto result = m_ptr;
        m_ptr = nullptr;
        return result;
    }

    /**
     * @brief Sets this pointer to @p ptr without modifying reference count.
     */
    void adopt(pointer ptr) {
        reset();
        m_ptr = ptr;
    }

    void reset(pointer new_value = nullptr) {
        if (m_ptr && !m_ptr->deref()) delete m_ptr;
        set_ptr(new_value);
    }

    intrusive_ptr& operator=(pointer ptr) {
        reset(ptr);
        return *this;
    }

    intrusive_ptr& operator=(intrusive_ptr&& other) {
        swap(other);
        return *this;
    }

    intrusive_ptr& operator=(const intrusive_ptr& other) {
        intrusive_ptr tmp{other};
        swap(tmp);
        return *this;
    }

    template<typename Y>
    intrusive_ptr& operator=(intrusive_ptr<Y> other) {
        intrusive_ptr tmp{std::move(other)};
        swap(tmp);
        return *this;
    }

    template<typename From>
    intrusive_ptr& operator=(const convertible<From, T*>& from) {
        reset(from.convert());
        return *this;
    }

    inline pointer get() const { return m_ptr; }
    inline pointer operator->() const { return m_ptr; }
    inline reference operator*() const { return *m_ptr; }

    inline bool operator!() const { return m_ptr == nullptr; }
    inline explicit operator bool() const { return m_ptr != nullptr; }

    inline ptrdiff_t compare(const_pointer ptr) const {
        return static_cast<ptrdiff_t>(get() - ptr);
    }

    inline ptrdiff_t compare(const intrusive_ptr& other) const {
        return compare(other.get());
    }

    inline ptrdiff_t compare(std::nullptr_t) const {
        return reinterpret_cast<ptrdiff_t>(get());
    }

    template<class C>
    intrusive_ptr<C> downcast() const {
        return (m_ptr) ? dynamic_cast<C*>(get()) : nullptr;
    }

    template<class C>
    intrusive_ptr<C> upcast() const {
        return (m_ptr) ? static_cast<C*>(get()) : nullptr;
    }

 private:

    pointer m_ptr;

    inline void set_ptr(pointer raw_ptr) {
        m_ptr = raw_ptr;
        if (raw_ptr) raw_ptr->ref();
    }

};

/**
 * @relates intrusive_ptr
 */
template<typename X, typename Y>
inline bool operator==(const intrusive_ptr<X>& lhs, const intrusive_ptr<Y>& rhs) {
    return lhs.get() == rhs.get();
}

/**
 * @relates intrusive_ptr
 */
template<typename X, typename Y>
inline bool operator!=(const intrusive_ptr<X>& lhs, const intrusive_ptr<Y>& rhs) {
    return !(lhs == rhs);
}

} // namespace cppa

#endif // CPPA_INTRUSIVE_PTR_HPP
