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
