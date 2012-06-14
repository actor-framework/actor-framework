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


#ifndef INTRUSIVE_PTR_HPP
#define INTRUSIVE_PTR_HPP

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
class intrusive_ptr : util::comparable<intrusive_ptr<T>, const T*>,
                      util::comparable<intrusive_ptr<T> > {

 public:

    constexpr intrusive_ptr() : m_ptr(nullptr) { }

    intrusive_ptr(T* raw_ptr) { set_ptr(raw_ptr); }

    intrusive_ptr(const intrusive_ptr& other) { set_ptr(other.m_ptr); }

    intrusive_ptr(intrusive_ptr&& other) : m_ptr(other.release()) { }

    // enables "actor_ptr s = self"
    template<typename From>
    intrusive_ptr(const convertible<From, T*>& from) {
        set_ptr(from.convert());
    }

    template<typename Y>
    intrusive_ptr(const intrusive_ptr<Y>& other) {
        static_assert(std::is_convertible<Y*, T*>::value,
                      "Y* is not assignable to T*");
        set_ptr(const_cast<Y*>(other.get()));
    }

    template<typename Y>
    intrusive_ptr(intrusive_ptr<Y>&& other) : m_ptr(other.release()) {
        static_assert(std::is_convertible<Y*, T*>::value,
                      "Y* is not assignable to T*");
    }

    ~intrusive_ptr() {
        if (m_ptr && !m_ptr->deref()) {
            delete m_ptr;
        }
    }

    inline T* get() { return m_ptr; }

    inline const T* get() const { return m_ptr; }

    T* release() {
        auto result = m_ptr;
        m_ptr = nullptr;
        return result;
    }

    inline void swap(intrusive_ptr& other) {
        std::swap(m_ptr, other.m_ptr);
    }

    void reset(T* new_value = nullptr) {
        if (m_ptr && !m_ptr->deref()) delete m_ptr;
        set_ptr(new_value);
    }

    intrusive_ptr& operator=(T* ptr) {
        reset(ptr);
        return *this;
    }

    intrusive_ptr& operator=(const intrusive_ptr& other) {
        intrusive_ptr tmp(other);
        swap(tmp);
        return *this;
    }

    intrusive_ptr& operator=(intrusive_ptr&& other) {
        reset();
        swap(other);
        return *this;
    }

    template<typename Y>
    intrusive_ptr& operator=(const intrusive_ptr<Y>& other) {
        static_assert(std::is_convertible<Y*, T*>::value,
                      "Y* is not assignable to T*");
        intrusive_ptr tmp(other);
        swap(tmp);
        return *this;
    }

    template<typename From>
    intrusive_ptr& operator=(const convertible<From, T*>& from) {
        reset(from.convert());
        return *this;
    }

    template<typename Y>
    intrusive_ptr& operator=(intrusive_ptr<Y>&& other) {
        static_assert(std::is_convertible<Y*, T*>::value,
                      "Y* is not assignable to T*");
        reset();
        m_ptr = other.release();
        return *this;
    }

    inline T& operator*() { return *m_ptr; }

    inline T* operator->() { return m_ptr; }

    inline const T& operator*() const { return *m_ptr; }

    inline const T* operator->() const { return m_ptr; }

    inline explicit operator bool() const { return m_ptr != nullptr; }

    inline ptrdiff_t compare(const T* ptr) const {
        return static_cast<ptrdiff_t>(get() - ptr);
    }

    inline ptrdiff_t compare(const intrusive_ptr& other) const {
        return compare(other.get());
    }

    template<class C>
    intrusive_ptr<C> downcast() const {
        return (m_ptr) ? dynamic_cast<C*>(const_cast<T*>(m_ptr)) : nullptr;
    }

    template<class C>
    intrusive_ptr<C> upcast() const {
        return (m_ptr) ? static_cast<C*>(const_cast<T*>(m_ptr)) : nullptr;
    }

 private:

    T* m_ptr;

    inline void set_ptr(T* raw_ptr) {
        m_ptr = raw_ptr;
        if (raw_ptr) raw_ptr->ref();
    }

};

/**
 * @relates intrusive_ptr
 */
template<typename X>
inline bool operator==(const intrusive_ptr<X>& ptr, decltype(nullptr)) {
    return ptr.get() == nullptr;
}

/**
 * @relates intrusive_ptr
 */
template<typename X>
inline bool operator==(decltype(nullptr), const intrusive_ptr<X>& ptr) {
    return ptr.get() == nullptr;
}

/**
 * @relates intrusive_ptr
 */
template<typename X>
inline bool operator!=(const intrusive_ptr<X>& lhs, decltype(nullptr)) {
    return lhs.get() != nullptr;
}

/**
 * @relates intrusive_ptr
 */
template<typename X>
inline bool operator!=(decltype(nullptr), const intrusive_ptr<X>& rhs) {
    return rhs.get() != nullptr;
}

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

#endif // INTRUSIVE_PTR_HPP
