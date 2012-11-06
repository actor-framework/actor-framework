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


#ifndef CPPA_INTRUSIVE_FWD_PTR_HPP
#define CPPA_INTRUSIVE_FWD_PTR_HPP

#include <memory>

#include "cppa/util/comparable.hpp"

namespace cppa {

/**
 * @brief A reference counting smart pointer implementation that
 *        can be used with forward declarated types.
 * @warning libcppa assumes T is derived from ref_counted and
 *          implicitly converts intrusive_fwd_ptr to intrusive_ptr
 * @relates intrusive_ptr
 * @relates ref_counted
 */
template<typename T, typename Ref, typename Deref>
class intrusive_fwd_ptr : util::comparable<intrusive_fwd_ptr<T,Ref,Deref> >,
                          util::comparable<intrusive_fwd_ptr<T,Ref,Deref>, const T*>,
                          util::comparable<intrusive_fwd_ptr<T,Ref,Deref>, std::nullptr_t> {

 public:

    typedef T*       pointer;
    typedef const T* const_pointer;
    typedef T        element_type;
    typedef T&       reference;
    typedef const T& const_reference;

    intrusive_fwd_ptr(pointer raw_ptr = nullptr, Ref ref = Ref(), Deref deref = Deref())
    : m_ref(std::move(ref)), m_deref(std::move(deref)) { set_ptr(raw_ptr); }

    intrusive_fwd_ptr(intrusive_fwd_ptr&& other)
    : m_ref(std::move(other.m_ref))
    , m_deref(std::move(m_deref))
    , m_ptr(other.release()) { }

    intrusive_fwd_ptr(const intrusive_fwd_ptr& other)
    : m_ref(other.m_ref), m_deref(other.m_deref) { set_ptr(other.get()); }

    ~intrusive_fwd_ptr() { if (m_ptr) m_deref(m_ptr); }

    inline void swap(intrusive_fwd_ptr& other) {
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
        if (m_ptr) m_deref(m_ptr);
        set_ptr(new_value);
    }

    template<typename... Args>
    void emplace(Args&&... args) {
        reset(new T(std::forward<Args>(args)...));
    }

    intrusive_fwd_ptr& operator=(pointer ptr) {
        reset(ptr);
        return *this;
    }

    intrusive_fwd_ptr& operator=(intrusive_fwd_ptr&& other) {
        swap(other);
        return *this;
    }

    intrusive_fwd_ptr& operator=(const intrusive_fwd_ptr& other) {
        intrusive_fwd_ptr tmp(other);
        swap(tmp);
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

    inline ptrdiff_t compare(const intrusive_fwd_ptr& other) const {
        return compare(other.get());
    }

    inline ptrdiff_t compare(std::nullptr_t) const {
        return reinterpret_cast<ptrdiff_t>(get());
    }

 private:

    Ref     m_ref;
    Deref   m_deref;
    pointer m_ptr;

    inline void set_ptr(pointer raw_ptr) {
        m_ptr = raw_ptr;
        if (raw_ptr) m_ref(raw_ptr);
    }

};

/**
 * @relates intrusive_ptr
 */
template<typename T, typename Ref, typename Deref>
inline bool operator==(const intrusive_fwd_ptr<T,Ref,Deref>& lhs,
                       const intrusive_fwd_ptr<T,Ref,Deref>& rhs) {
    return lhs.get() == rhs.get();
}

/**
 * @relates intrusive_ptr
 */
template<typename T, typename Ref, typename Deref>
inline bool operator!=(const intrusive_fwd_ptr<T,Ref,Deref>& lhs,
                       const intrusive_fwd_ptr<T,Ref,Deref>& rhs) {
    return !(lhs == rhs);
}

} // namespace cppa

#endif // CPPA_INTRUSIVE_FWD_PTR_HPP
