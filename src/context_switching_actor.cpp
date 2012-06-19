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


#include "cppa/context_switching_actor.hpp"
#ifndef CPPA_DISABLE_CONTEXT_SWITCHING

#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/self.hpp"

namespace cppa {

context_switching_actor::context_switching_actor()
: m_fiber(&context_switching_actor::trampoline, this) {
}

context_switching_actor::context_switching_actor(std::function<void()> fun)
: m_fiber(&context_switching_actor::trampoline, this), m_behavior(fun) {
}

void context_switching_actor::run() {
    if (m_behavior) m_behavior();
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

detail::recursive_queue_node* context_switching_actor::receive_node() {
    detail::recursive_queue_node* e = m_mailbox.try_pop();
    while (e == nullptr) {
        if (m_mailbox.can_fetch_more() == false) {
            m_state.store(abstract_scheduled_actor::about_to_block);
            // make sure mailbox is empty
            if (m_mailbox.can_fetch_more()) {
                // someone preempt us => continue
                m_state.store(abstract_scheduled_actor::ready);
            }
            else {
                // wait until actor becomes rescheduled
                detail::yield(detail::yield_state::blocked);
            }
        }
        e = m_mailbox.try_pop();
    }
    return e;
}

void context_switching_actor::dequeue(partial_function& fun) {
    m_recv_policy.receive(this, fun);
}

void context_switching_actor::dequeue(behavior& bhvr) {
    if (bhvr.timeout().valid() == false) {
        m_recv_policy.receive(this, bhvr.get_partial_function());
    }
    else if (m_recv_policy.invoke_from_cache(this, bhvr) == false) {
        if (bhvr.timeout().is_zero()) {
            for (auto e = m_mailbox.try_pop(); e != 0; e = m_mailbox.try_pop()) {
                CPPA_REQUIRE(e->marked == false);
                if (m_recv_policy.invoke(this, e, bhvr)) return;
            }
            bhvr.handle_timeout();
        }
        else {
            request_timeout(bhvr.timeout());
            while (m_recv_policy.invoke(this, receive_node(), bhvr) == false) { }
        }
    }
}

resume_result context_switching_actor::resume(util::fiber* from) {
    using namespace detail;
    self.set(this);
    for (;;) {
        switch (call(&m_fiber, from)) {
            case yield_state::done: {
                return resume_result::actor_done;
            }
            case yield_state::ready: {
                break;
            }
            case yield_state::blocked: {
                switch (compare_exchange_state(abstract_scheduled_actor::about_to_block,
                                               abstract_scheduled_actor::blocked)) {
                    case abstract_scheduled_actor::ready: {
                        break;
                    }
                    case abstract_scheduled_actor::blocked: {
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

void context_switching_actor::unbecome() {
    if (m_stack_ptr) {
        m_stack_ptr->pop_back();
    }
    else {
        quit();
    }
}

void context_switching_actor::do_become(behavior* bhvr, bool ownership, bool discard) {
    if (m_stack_ptr) {
        if (discard) m_stack_ptr->pop_back();
        m_stack_ptr->push_back(bhvr, ownership);
    }
    else {
        m_stack_ptr.reset(new detail::behavior_stack);
        m_stack_ptr->exec();
        quit();
    }
}


} // namespace cppa

#else // ifdef CPPA_DISABLE_CONTEXT_SWITCHING

namespace { int keep_compiler_happy() { return 42; } }

#endif // ifdef CPPA_DISABLE_CONTEXT_SWITCHING
