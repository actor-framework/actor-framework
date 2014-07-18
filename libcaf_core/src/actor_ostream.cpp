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
#include "caf/scheduler.hpp"
#include "caf/local_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/actor_ostream.hpp"

#include "caf/detail/singletons.hpp"

namespace caf {

actor_ostream::actor_ostream(actor self) : m_self(std::move(self)) {
    m_printer = detail::singletons::get_scheduling_coordinator()->printer();
}

actor_ostream& actor_ostream::write(std::string arg) {
    send_as(m_self, m_printer, atom("add"), std::move(arg));
    return *this;
}

actor_ostream& actor_ostream::flush() {
    send_as(m_self, m_printer, atom("flush"));
    return *this;
}

} // namespace caf

namespace std {

caf::actor_ostream& endl(caf::actor_ostream& o) { return o.write("\n"); }

caf::actor_ostream& flush(caf::actor_ostream& o) { return o.flush(); }

} // namespace std
