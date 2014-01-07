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


#include "cppa/policy.hpp"
#include "cppa/singletons.hpp"
#include "cppa/scoped_actor.hpp"

#include "cppa/detail/proper_actor.hpp"
#include "cppa/detail/actor_registry.hpp"

namespace cppa {

namespace {

struct impl : blocking_untyped_actor {
    void act() override { }
};

blocking_untyped_actor* alloc() {
    using namespace policy;
    return new detail::proper_actor<impl,
                                    no_scheduling,
                                    not_prioritizing,
                                    no_resume,
                                    nestable_invoke>;
}

} // namespace <anonymous>

scoped_actor::scoped_actor() : m_self(alloc()) {
    m_prev = CPPA_SET_AID(m_self->id());
    get_actor_registry()->inc_running();
}

scoped_actor::~scoped_actor() {
    get_actor_registry()->dec_running();
    CPPA_SET_AID(m_prev);
}

} // namespace cppa
