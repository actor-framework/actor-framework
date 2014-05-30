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


#ifndef CPPA_CPPA_FWD_HPP
#define CPPA_CPPA_FWD_HPP

#include <cstdint>

namespace cppa {

// classes
class actor;
class group;
class channel;
class node_id;
class behavior;
class resumable;
class any_tuple;
class actor_addr;
class actor_proxy;
class scoped_actor;
class execution_unit;
class abstract_actor;
class abstract_group;
class blocking_actor;
class message_header;
class partial_function;
class uniform_type_info;
class event_based_actor;
class primitive_variant;

// structs
struct anything;

// enums
enum primitive_type : unsigned char;
enum class atom_value : std::uint64_t;

// class templates
template<typename> class optional;
template<typename> class intrusive_ptr;
template<typename> class weak_intrusive_ptr;

// intrusive pointer typedefs
typedef intrusive_ptr<abstract_group>      abstract_group_ptr;
typedef intrusive_ptr<actor_proxy>         actor_proxy_ptr;
typedef intrusive_ptr<node_id>             node_id_ptr;

// weak intrusive pointer typedefs
typedef weak_intrusive_ptr<actor_proxy>    weak_actor_proxy_ptr;

// convenience typedefs
typedef const message_header& msg_hdr_cref;

} // namespace cppa

#endif // CPPA_CPPA_FWD_HPP
