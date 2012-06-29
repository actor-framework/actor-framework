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


#include <typeinfo>

#include "cppa/primitive_variant.hpp"
#include "cppa/detail/type_to_ptype.hpp"

namespace cppa {

namespace {

template<class T>
void ptv_del(T&,
             typename std::enable_if<std::is_arithmetic<T>::value>::type* = 0) {
    // arithmetic types don't need destruction
}

template<class T>
void ptv_del(T& what,
             typename std::enable_if<!std::is_arithmetic<T>::value>::type* = 0) {
    what.~T();
}

struct destroyer {
    template<typename T>
    inline void operator()(T& what) const {
        ptv_del(what);
    }
};

struct type_reader {
    const std::type_info* tinfo;
    type_reader() : tinfo(nullptr) { }
    template<typename T>
    void operator()(const T&) {
        tinfo = &typeid(T);
    }
};

struct comparator {
    bool result;
    const primitive_variant& lhs;
    const primitive_variant& rhs;

    comparator(const primitive_variant& pv1, const primitive_variant& pv2)
        : result(false), lhs(pv1), rhs(pv2) {
    }

    template<primitive_type PT>
    void operator()(util::pt_token<PT>) {
        if (rhs.ptype() == PT) {
            result = (get<PT>(lhs) == get<PT>(rhs));
        }
    }
};

struct initializer {
    primitive_variant& lhs;

    inline initializer(primitive_variant& pv) : lhs(pv) { }

    template<primitive_type PT>
    inline void operator()(util::pt_token<PT>) {
        typedef typename detail::ptype_to_type<PT>::type T;
        lhs = T();
    }
};

struct setter {
    primitive_variant& lhs;

    setter(primitive_variant& pv) : lhs(pv) { }

    template<typename T>
    inline void operator()(const T& rhs) {
        lhs = rhs;
    }
};

struct mover {
    primitive_variant& lhs;

    mover(primitive_variant& pv) : lhs(pv) { }

    template<typename T>
    inline void operator()(T& rhs) {
        lhs = std::move(rhs);
    }
};

} // namespace <anonymous>

primitive_variant::primitive_variant() : m_ptype(pt_null) { }

primitive_variant::primitive_variant(primitive_type ptype) : m_ptype(pt_null) {
    util::pt_dispatch(ptype, initializer(*this));
}

primitive_variant::primitive_variant(const primitive_variant& other)
    : m_ptype(pt_null) {
    other.apply(setter(*this));
}

primitive_variant::primitive_variant(primitive_variant&& other)
    : m_ptype(pt_null) {
    other.apply(mover(*this));
}

primitive_variant& primitive_variant::operator=(const primitive_variant& other) {
    other.apply(setter(*this));
    return *this;
}

primitive_variant& primitive_variant::operator=(primitive_variant&& other) {
    other.apply(mover(*this));
    return *this;
}

bool equal(const primitive_variant& lhs, const primitive_variant& rhs) {
    comparator cmp(lhs, rhs);
    util::pt_dispatch(lhs.m_ptype, cmp);
    return cmp.result;
}

const std::type_info& primitive_variant::type() const {
    type_reader tr;
    apply(tr);
    return (tr.tinfo == nullptr) ? typeid(void) : *tr.tinfo;
}

primitive_variant::~primitive_variant() {
    destroy();
}

void primitive_variant::destroy() {
    apply(destroyer());
    m_ptype = pt_null;
}

} // namespace cppa
