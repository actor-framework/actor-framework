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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_DETAIL_OBJECT_IMPL_HPP
#define CPPA_DETAIL_OBJECT_IMPL_HPP

#include "cppa/object.hpp"

namespace cppa {

template<typename T>
const utype& uniform_type_info();

}

namespace cppa {
namespace detail {

template<typename T>
struct obj_impl : object {
    T m_value;
    obj_impl() : m_value() { }
    obj_impl(const T& v) : m_value(v) { }
    virtual object* copy() const { return new obj_impl(m_value); }
    virtual const utype& type() const { return uniform_type_info<T>(); }
    virtual void* mutable_value() { return &m_value; }
    virtual const void* value() const { return &m_value; }
    virtual void serialize(serializer& s) const {
        s << m_value;
    }
    virtual void deserialize(deserializer& d) {
        d >> m_value;
    }
};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_OBJECT_IMPL_HPP
