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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
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


#include "cppa/config.hpp"

#include <atomic>
#include <iostream>

#include "cppa/self.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/attachable.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/scheduled_actor.hpp"

#include "cppa/detail/thread.hpp"
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/converted_thread_context.hpp"

using std::cout;
using std::endl;

namespace {

void run_actor(cppa::intrusive_ptr<cppa::local_actor> m_self,
               cppa::scheduled_actor* behavior)
{
    cppa::self.set(m_self.get());
    if (behavior)
    {
        try { behavior->act(); }
        catch (...) { }
        try { behavior->on_exit(); }
        catch (...) { }
        delete behavior;
        cppa::self.set(nullptr);
    }
    cppa::detail::dec_actor_count();
}

} // namespace <anonymous>

namespace cppa { namespace detail {

actor_ptr mock_scheduler::spawn(scheduled_actor* behavior)
{
    inc_actor_count();
    CPPA_MEMORY_BARRIER();
    intrusive_ptr<local_actor> ctx(new detail::converted_thread_context);
    thread(run_actor, ctx, behavior).detach();
    return ctx;
}

actor_ptr mock_scheduler::spawn(abstract_event_based_actor* what)
{
    // TODO: don't delete what :)
    delete what;
    return nullptr;
}

actor_ptr mock_scheduler::spawn(scheduled_actor* behavior, scheduling_hint)
{
    return spawn(behavior);
}

} } // namespace detail
