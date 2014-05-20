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
 * Copyright (C) 2011-2013                                                    *
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


#ifndef CPPA_DETAIL_HANDLE_HPP
#define CPPA_DETAIL_HANDLE_HPP

#include "cppa/util/comparable.hpp"

namespace cppa {
namespace detail {

template<typename Subtype>
class handle : util::comparable<Subtype> {

 public:

    inline handle() : m_id{-1} { }

    handle(const Subtype& other) {
        m_id = other.id();
    }

    handle(const handle& other) : m_id(other.m_id) { }

    Subtype& operator=(const handle& other) {
        m_id = other.id();
        return *static_cast<Subtype*>(this);
    }

    inline int id() const {
        return m_id;
    }

    inline int compare(const Subtype& other) const {
        return m_id - other.id();
    }

    inline bool invalid() const {
        return m_id == -1;
    }

    static inline Subtype from_int(int id) {
        return {id};
    }


 protected:

    inline handle(int handle_id) : m_id{handle_id} { }

 private:

    int m_id;

};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_HANDLE_HPP
