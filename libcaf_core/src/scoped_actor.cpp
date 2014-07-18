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

#include "caf/scoped_actor.hpp"

#include "caf/policy/no_resume.hpp"
#include "caf/policy/no_scheduling.hpp"
#include "caf/policy/actor_policies.hpp"
#include "caf/policy/nestable_invoke.hpp"
#include "caf/policy/not_prioritizing.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/proper_actor.hpp"
#include "caf/detail/actor_registry.hpp"

namespace caf {

namespace {

struct impl : blocking_actor {
    void act() override { }
};

blocking_actor* alloc() {
    using namespace policy;
    return new detail::proper_actor<impl, actor_policies<no_scheduling,
                                                         not_prioritizing,
                                                         no_resume,
                                                         nestable_invoke>>;
}

} // namespace <anonymous>

void scoped_actor::init(bool hide_actor) {
    m_hidden = hide_actor;
    m_self.reset(alloc());
    if (!m_hidden) {
        detail::singletons::get_actor_registry()->inc_running();
        m_prev = CAF_SET_AID(m_self->id());
    }
}

scoped_actor::scoped_actor() {
    init(false);
}

scoped_actor::scoped_actor(bool hide_actor) {
    init(hide_actor);
}

scoped_actor::~scoped_actor() {
    auto r = m_self->planned_exit_reason();
    m_self->cleanup(r == exit_reason::not_exited ? exit_reason::normal : r);
    if (!m_hidden) {
        detail::singletons::get_actor_registry()->dec_running();
        CAF_SET_AID(m_prev);
    }
}

} // namespace caf
