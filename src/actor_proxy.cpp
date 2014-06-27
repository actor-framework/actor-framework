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


#include <utility>
#include <iostream>

#include "cppa/atom.hpp"
#include "cppa/to_string.hpp"
#include "cppa/message.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/singletons.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/exit_reason.hpp"

#include "cppa/io/middleman.hpp"

#include "cppa/detail/types_array.hpp"
#include "cppa/detail/singleton_manager.hpp"

using namespace std;

namespace cppa {

actor_proxy::~actor_proxy() { }

actor_proxy::actor_proxy(actor_id mid) : super(mid) {
    m_node = get_middleman()->node();
}

} // namespace cppa
