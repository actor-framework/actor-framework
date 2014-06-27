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


#ifndef CPPA_TO_STRING_HPP
#define CPPA_TO_STRING_HPP

#include <type_traits>

#include "cppa/atom.hpp" // included for to_string(atom_value)
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/node_id.hpp"
#include "cppa/anything.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/abstract_group.hpp"
#include "cppa/message_header.hpp"
#include "cppa/uniform_type_info.hpp"

namespace std { class exception; }

namespace cppa {

namespace detail {

std::string to_string_impl(const void* what, const uniform_type_info* utype);

template<typename T>
inline std::string to_string_impl(const T& what) {
    return to_string_impl(&what, uniform_typeid<T>());
}

} // namespace detail

inline std::string to_string(const any_tuple& what) {
    return detail::to_string_impl(what);
}

inline std::string to_string(msg_hdr_cref what) {
    return detail::to_string_impl(what);
}

inline std::string to_string(const actor& what) {
    return detail::to_string_impl(what);
}

inline std::string to_string(const actor_addr& what) {
    return detail::to_string_impl(what);
}

inline std::string to_string(const group& what) {
    return detail::to_string_impl(what);
}

inline std::string to_string(const channel& what) {
    return detail::to_string_impl(what);
}

// implemented in node_id.cpp
std::string to_string(const node_id& what);

// implemented in node_id.cpp
std::string to_string(const node_id_ptr& what);

/**
 * @brief Converts @p e to a string including the demangled type of e
 *        and @p e.what().
 */
std::string to_verbose_string(const std::exception& e);

} // namespace cppa

#endif // CPPA_TO_STRING_HPP
