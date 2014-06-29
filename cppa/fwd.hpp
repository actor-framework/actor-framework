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

#ifndef CPPA_FWD_HPP
#define CPPA_FWD_HPP

#include <cstdint>

namespace cppa {

template<class>
class intrusive_ptr;

// classes
class actor;
class group;
class channel;
class node_id;
class behavior;
class resumable;
class message;
class actor_addr;
class local_actor;
class actor_proxy;
class scoped_actor;
class execution_unit;
class abstract_actor;
class abstract_group;
class blocking_actor;
class message_handler;
class uniform_type_info;
class event_based_actor;

// structs
struct anything;
struct invalid_actor_t;
struct invalid_actor_addr_t;

// enums
enum primitive_type : unsigned char;
enum class atom_value : uint64_t;

// intrusive pointer typedefs
typedef intrusive_ptr<abstract_group> abstract_group_ptr;
typedef intrusive_ptr<actor_proxy> actor_proxy_ptr;

// functions
template<typename T, typename U>
T actor_cast(const U&);

namespace io {

class broker;

} // namespace io

} // namespace cppa

#endif // CPPA_FWD_HPP
