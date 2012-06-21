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

#include <thread>
#include <atomic>
#include <iostream>

#include "cppa/self.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/attachable.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/scheduled_actor.hpp"
#include "cppa/thread_mapped_actor.hpp"
#include "cppa/event_based_actor.hpp"

#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/to_uniform_name.hpp"

using std::cout;
using std::cerr;
using std::endl;

namespace cppa { namespace detail {

typedef intrusive_ptr<thread_mapped_actor> thread_mapped_actor_ptr;

namespace {

void run_actor(thread_mapped_actor_ptr m_self) {
    self.set(m_self.get());
    try {
        m_self->init();
        m_self->initialized(true);
        m_self->run();
        m_self->on_exit();
    }
    catch (...) { }
    self.set(nullptr);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    dec_actor_count();
}

void run_hidden_actor(local_actor_ptr m_self,
                      std::function<void()> what) {
    self.set(m_self.get());
    try { what(); }
    catch (...) { }
    self.set(nullptr);
}

} // namespace <anonymous>

std::thread mock_scheduler::spawn_hidden_impl(std::function<void()> what,
                                              local_actor_ptr ctx) {
    return std::thread{run_hidden_actor, std::move(ctx), std::move(what)};
}

actor_ptr mock_scheduler::spawn_impl(std::function<void()> what) {
    inc_actor_count();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    thread_mapped_actor_ptr ctx{new thread_mapped_actor(std::move(what))};
    std::thread{run_actor, ctx}.detach();
    return std::move(ctx);
}

actor_ptr mock_scheduler::spawn(scheduled_actor*) {
    cerr << "mock_scheduler::spawn(scheduled_actor*)" << endl;
    abort();
    return nullptr;
}

actor_ptr mock_scheduler::spawn(std::function<void()> what, scheduling_hint) {
    return spawn_impl(what);
}

void mock_scheduler::enqueue(scheduled_actor*) {
    cerr << "mock_scheduler::enqueue(scheduled_actor)" << endl;
    abort();
}

} } // namespace detail
