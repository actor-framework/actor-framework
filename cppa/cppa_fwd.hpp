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


#ifndef CPPA_FWD_HPP
#define CPPA_FWD_HPP

#include <cstdint>

namespace cppa {

// classes
class actor;
class group;
class channel;
class behavior;
class any_tuple;
class self_type;
class message_header;
class partial_function;
class uniform_type_info;
class primitive_variant;
class process_information;

// structs
struct anything;

// enums
enum primitive_type : unsigned char;
enum class atom_value : std::uint64_t;

// class templates
template<typename> class option;
template<typename> class intrusive_ptr;

// typedefs
typedef intrusive_ptr<actor>               actor_ptr;
typedef intrusive_ptr<group>               group_ptr;
typedef intrusive_ptr<channel>             channel_ptr;
typedef intrusive_ptr<process_information> process_information_ptr;

} // namespace cppa

#endif // CPPA_FWD_HPP
