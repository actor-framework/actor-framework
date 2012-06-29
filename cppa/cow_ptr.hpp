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


#ifndef CPPA_COW_PTR_HPP
#define CPPA_COW_PTR_HPP

#include <cstddef>
#include <stdexcept>
#include <type_traits>

#include "cppa/intrusive_ptr.hpp"
#include "cppa/util/comparable.hpp"

namespace cppa {

/**
 * @ingroup CopyOnWrite
 * @brief A copy-on-write smart pointer implementation.
 * @tparam T A class that provides a copy() member function and has
 *           the same interface as or is a subclass of
 *           {@link ref_counted}.
 */
template<typename T>
class cow_ptr : util::comparable<cow_ptr<T> >,
                util::comparable<cow_ptr<T>, const T*>,
                util::comparable<cow_ptr<T>, std::nullptr_t> {

 public:

    typedef T*       pointer;
    typedef const T* const_pointer;
    typedef T        element_type;
    typedef T&       reference;
    typedef const T& const_reference;

    constexpr cow_ptr() : m_ptr() { }

    cow_ptr(pointer raw_ptr) : m_ptr(raw_ptr) { }

    cow_ptr(cow_ptr&&) = default;
    cow_ptr(const cow_ptr&) = default;
    cow_ptr& operator=(cow_ptr&&) = default;
    cow_ptr& operator=(const cow_ptr&) = default;

    template<typename Y>
    cow_ptr(cow_ptr<Y> other) : m_ptr(other.m_ptr.release()) {
        static_assert(std::is_convertible<Y*, T*>::value,
                      "Y* is not assignable to T*");
    }

    inline void swap(cow_ptr& other) { m_ptr.swap(other.m_ptr); }

    template<typename Y>
    cow_ptr& operator=(cow_ptr<Y> other) {
        m_ptr = std::move(other.m_ptr);
        return *this;
    }

    void detach() { static_cast<void>(get_detached()); }

    inline void reset(T* value = nullptr) { m_ptr.reset(value); }

    // non-const access (detaches this pointer)
    inline pointer get() { return (m_ptr) ? get_detached() : nullptr; }
    inline pointer operator->() { return get_detached(); }
    inline reference operator*() { return *get_detached(); }

    // const access (does not detach this pointer)
    inline const_pointer get() const { return m_ptr.get(); }
    inline const_pointer operator->() const { return get(); }
    inline const_reference operator*() const { return *get(); }
    inline explicit operator bool() const { return static_cast<bool>(m_ptr); }

    template<typename Arg>
    inline ptrdiff_t compare(Arg&& what) const {
        return m_ptr.compare(std::forward<Arg>(what));
    }

 private:

    intrusive_ptr<T> m_ptr;

    pointer get_detached() {
        auto ptr = m_ptr.get();
        if (!ptr->unique()) {
            pointer new_ptr = ptr->copy();
            reset(new_ptr);
            return new_ptr;
        }
        return ptr;
    }

};

/**
 * @relates cow_ptr
 */
template<typename X, typename Y>
inline bool operator==(const cow_ptr<X>& lhs, const cow_ptr<Y>& rhs) {
    return lhs.get() == rhs.get();
}

/**
 * @relates cow_ptr
 */
template<typename X, typename Y>
inline bool operator!=(const cow_ptr<X>& lhs, const cow_ptr<Y>& rhs) {
    return !(lhs == rhs);
}

} // namespace cppa

#endif // CPPA_COW_PTR_HPP
