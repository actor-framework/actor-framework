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


#include "cppa/cppa.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/singletons.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/scoped_actor.hpp"
#include "cppa/actor_ostream.hpp"

namespace cppa {

actor_ostream::actor_ostream(actor self) : m_self(std::move(self)) {
    m_printer = get_scheduling_coordinator()->printer();
}

actor_ostream& actor_ostream::write(std::string arg) {
    send_as(m_self, m_printer, atom("add"), move(arg));
    return *this;
}

actor_ostream& actor_ostream::flush() {
    send_as(m_self, m_printer, atom("flush"));
    return *this;
}

} // namespace cppa

namespace std {

cppa::actor_ostream& endl(cppa::actor_ostream& o) {
    return o.write("\n");
}

cppa::actor_ostream& flush(cppa::actor_ostream& o) {
    return o.flush();
}

} // namespace std
