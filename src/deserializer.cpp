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


#include <string>

#include "cppa/deserializer.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/buffer.hpp"

#include "cppa/detail/to_uniform_name.hpp"

namespace cppa {

deserializer::deserializer(actor_namespace* ns, type_lookup_table* ot)
: m_namespace{ns}, m_incoming_types{ot} { }

deserializer::~deserializer() { }

void deserializer::read_raw(size_t num_bytes, util::buffer& storage) {
    storage.acquire(num_bytes);
    read_raw(num_bytes, storage.data());
    storage.inc_size(num_bytes);
}

} // namespace cppa
