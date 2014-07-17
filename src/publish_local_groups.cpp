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

#include "caf/all.hpp"

#include "caf/io/publish.hpp"
#include "caf/io/publish_local_groups.hpp"

namespace caf {
namespace io {

namespace {
struct group_nameserver : event_based_actor {
    behavior make_behavior() override {
        return (
            on(atom("GET_GROUP"), arg_match) >> [](const std::string& name) {
                return make_message(atom("GROUP"), group::get("local", name));
            },
            on(atom("SHUTDOWN")) >> [=] {
                quit();
            }
        );
    }

};
} // namespace <anonymous>

void publish_local_groups(uint16_t port, const char* addr) {
    auto gn = spawn<group_nameserver, hidden>();
    try {
        publish(gn, port, addr);
    }
    catch (std::exception&) {
        gn->enqueue(invalid_actor_addr, message_id::invalid,
                    make_message(atom("SHUTDOWN")),
                    nullptr);
        throw;
    }
}

} // namespace io
} // namespace caf
