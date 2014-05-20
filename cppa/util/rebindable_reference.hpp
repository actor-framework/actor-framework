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
 * Copyright (C) 2011-2014                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef CPPA_UTIL_REBINDABLE_REFERENCE_HPP
#define CPPA_UTIL_REBINDABLE_REFERENCE_HPP

#include "cppa/config.hpp"

#include "cppa/util/type_traits.hpp"

namespace cppa {
namespace util {

template<typename T>
struct call_helper {
    typedef typename map_to_result_type<T>::type result_type;
    template<typename... Ts>
    result_type operator()(T& f, const Ts&... args) const {
        return f(std::forward<Ts>(args)...);
    }
};

template<>
struct call_helper<bool> {
    typedef bool result_type;
    bool operator()(const bool& value) const {
        return value;
    }
};

template<>
struct call_helper<const bool> : call_helper<bool> { };

/**
 * @brief A reference wrapper similar to std::reference_wrapper, but
 *        allows rebinding the reference. Note that this wrapper can
 *        be 'dangling', because it provides a default constructor.
 */
template<typename T>
class rebindable_reference {

 public:

    rebindable_reference() : m_ptr(nullptr) { }

    rebindable_reference(T& value) : m_ptr(&value) { }

    template<typename U>
    rebindable_reference(const rebindable_reference<U>& other) : m_ptr(other.ptr()) { }

    inline bool bound() const {
        return m_ptr != nullptr;
    }

    inline void rebind(T& value) {
        m_ptr  = &value;
    }

    template<typename U>
    inline void rebind(const rebindable_reference<U>& other) {
        m_ptr  = other.ptr();
    }

    inline T* ptr() const {
        CPPA_REQUIRE(bound());
        return m_ptr;
    }

    inline T& get() const {
        CPPA_REQUIRE(bound());
        return *m_ptr;
    }

    inline operator T&() const {
        CPPA_REQUIRE(bound());
        return *m_ptr;
    }

    template<typename... Ts>
    typename call_helper<T>::result_type operator()(Ts&&... args) {
        call_helper<T> f;
        return f(get(), std::forward<Ts>(args)...);
    }

 private:

    T* m_ptr;

};

template<typename T>
T& unwrap_ref(T& ref) {
    return ref;
}

template<typename T>
T& unwrap_ref(util::rebindable_reference<T>& ref) {
    return ref.get();
}

template<typename T>
typename std::add_const<T>::type&
unwrap_ref(const util::rebindable_reference<T>& ref) {
    return ref.get();
}

} // namespace util
} // namespace cppa


#endif // CPPA_UTIL_REBINDABLE_REFERENCE_HPP
