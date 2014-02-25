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


#include "cppa/context_switching_actor.hpp"
#ifndef CPPA_DISABLE_CONTEXT_SWITCHING

#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/self.hpp"

namespace cppa {

context_switching_actor::context_switching_actor(std::function<void()> fun)
: super(actor_state::ready, true)
, m_fiber(&context_switching_actor::trampoline, this) {
    set_behavior(std::move(fun));
}

auto context_switching_actor::init_timeout(const util::duration& tout) -> timeout_type {
    // request explicit timeout message
    request_timeout(tout);
    return {};
}

mailbox_element* context_switching_actor::await_message(const timeout_type&) {
    // receives requested timeout message if timeout occured
    return await_message();
}

mailbox_element* context_switching_actor::await_message() {
    auto e = m_mailbox.try_pop();
    while (e == nullptr) {
        if (m_mailbox.can_fetch_more() == false) {
            set_state(actor_state::about_to_block);
            // make sure mailbox is empty
            if (m_mailbox.can_fetch_more()) {
                // someone preempt us => continue
                set_state(actor_state::ready);
            }
            // wait until actor becomes rescheduled
            else detail::yield(detail::yield_state::blocked);
        }
        e = m_mailbox.try_pop();
    }
    return e;
}

void context_switching_actor::trampoline(void* this_ptr) {
    auto _this = reinterpret_cast<context_switching_actor*>(this_ptr);
    bool cleanup_called = false;
    try { _this->run(); }
    catch (actor_exited&) {
        // cleanup already called by scheduled_actor::quit
        cleanup_called = true;
    }
    catch (...) {
        _this->cleanup(exit_reason::unhandled_exception);
        cleanup_called = true;
    }
    if (!cleanup_called) _this->cleanup(exit_reason::normal);
    _this->on_exit();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    detail::yield(detail::yield_state::done);
}

scheduled_actor_type context_switching_actor::impl_type() {
    return context_switching_impl;
}

resume_result context_switching_actor::resume(util::fiber* from, actor_ptr& next_job) {
    CPPA_LOGMF(CPPA_TRACE, this, "state = " << static_cast<int>(state()));
    CPPA_REQUIRE(from != nullptr);
    CPPA_REQUIRE(next_job == nullptr);
    using namespace detail;
    scoped_self_setter sss{this};
    for (;;) {
        switch (call(&m_fiber, from)) {
            case yield_state::done: {
                CPPA_REQUIRE(next_job == nullptr);
                m_chained_actor.swap(next_job);
                return resume_result::actor_done;
            }
            case yield_state::ready: {
                break;
            }
            case yield_state::blocked: {
                CPPA_REQUIRE(next_job == nullptr);
                m_chained_actor.swap(next_job);
                CPPA_REQUIRE(m_chained_actor == nullptr);
                switch (compare_exchange_state(actor_state::about_to_block,
                                               actor_state::blocked)) {
                    case actor_state::ready: {
                        // restore variables
                        m_chained_actor.swap(next_job);
                        CPPA_REQUIRE(next_job == nullptr);
                        break;
                    }
                    case actor_state::blocked: {
                        // wait until someone re-schedules that actor
                        return resume_result::actor_blocked;
                    }
                    default: {
                        CPPA_CRITICAL("illegal yield result");
                    }
                }
                break;
            }
            default: {
                CPPA_CRITICAL("illegal state");
            }
        }
    }
}

} // namespace cppa

#else // ifdef CPPA_DISABLE_CONTEXT_SWITCHING

namespace cppa { int keep_compiler_happy_function() { return 42; } }

#endif // ifdef CPPA_DISABLE_CONTEXT_SWITCHING
