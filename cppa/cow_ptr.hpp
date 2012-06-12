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


#ifndef COW_PTR_HPP
#define COW_PTR_HPP

#include <stdexcept>
#include <type_traits>

#include "cppa/intrusive_ptr.hpp"

namespace cppa {

/**
 * @ingroup CopyOnWrite
 * @brief A copy-on-write smart pointer implementation.
 * @tparam T A class that provides a copy() member function and has
 *           the same interface as or is a subclass of
 *           {@link ref_counted}.
 */
template<typename T>
class cow_ptr {

 public:

    template<typename Y>
    explicit cow_ptr(Y* raw_ptr) : m_ptr(raw_ptr) { }

    explicit cow_ptr(T* raw_ptr) : m_ptr(raw_ptr) { }

    cow_ptr(cow_ptr&& other) : m_ptr(std::move(other.m_ptr)) { }

    cow_ptr(const cow_ptr& other) : m_ptr(other.m_ptr) { }

    template<typename Y>
    cow_ptr(const cow_ptr<Y>& other) : m_ptr(const_cast<Y*>(other.get())) { }

    inline void swap(cow_ptr& other) {
        m_ptr.swap(other.m_ptr);
    }

    cow_ptr& operator=(cow_ptr&& other) {
        swap(other);
        return *this;
    }

    cow_ptr& operator=(const cow_ptr& other) {
        cow_ptr tmp{other};
        swap(tmp);
        return *this;
    }

    template<typename Y>
    cow_ptr& operator=(const cow_ptr<Y>& other) {
        cow_ptr tmp{other};
        swap(tmp);
        return *this;
    }

    void detach() { (void) detached_ptr();
    }

    inline void reset(T* value = nullptr) { m_ptr.reset(value); }

    inline T* get() { return (m_ptr) ? detached_ptr() : nullptr; }

    inline T& operator*() { return *detached_ptr(); }

    inline T* operator->() { return detached_ptr(); }

    inline const T* get() const { return ptr(); }

    inline const T& operator*() const { return *ptr(); }

    inline const T* operator->() const { return ptr(); }

    inline explicit operator bool() const { return static_cast<bool>(m_ptr); }

 private:

    intrusive_ptr<T> m_ptr;

    T* detached_ptr() {
        T* ptr = m_ptr.get();
        if (!ptr->unique()) {
            //T* new_ptr = detail::copy_of(ptr, copy_of_token());
            T* new_ptr = ptr->copy();
            cow_ptr tmp(new_ptr);
            swap(tmp);
            return new_ptr;
        }
        return ptr;
    }

    inline const T* ptr() const { return m_ptr.get(); }

};

} // namespace cppa

#endif // COW_PTR_HPP
