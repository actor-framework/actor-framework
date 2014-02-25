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


#include <iostream>
#include "cppa/to_string.hpp"

#include "cppa/cppa.hpp"
#include "cppa/self.hpp"
#include "cppa/logging.hpp"
#include "cppa/event_based_actor.hpp"

using namespace std;

namespace cppa {

class default_scheduled_actor : public event_based_actor {

    typedef event_based_actor super;

 public:

    typedef std::function<void()> fun_type;

    default_scheduled_actor(fun_type&& fun)
    : super(actor_state::ready), m_fun(std::move(fun)), m_initialized(false) { }

    void init() { }

    resume_result resume(util::fiber* f, actor_ptr& next) {
        if (!m_initialized) {
            scoped_self_setter sss{this};
            m_initialized = true;
            m_fun();
            if (m_bhvr_stack.empty()) {
                if (exit_reason() == exit_reason::not_exited) {
                    quit(exit_reason::normal);
                }
                else {
                    set_state(actor_state::done);
                    m_bhvr_stack.clear();
                    m_bhvr_stack.cleanup();
                    on_exit();
                    next.swap(m_chained_actor);
                    set_state(actor_state::done);
                }
                return resume_result::actor_done;
            }
        }
        return event_based_actor::resume(f, next);
    }

    scheduled_actor_type impl_type() {
        return default_event_based_impl;
    }

 private:

    fun_type m_fun;
    bool m_initialized;

};

intrusive_ptr<event_based_actor> event_based_actor::from(std::function<void()> fun) {
    return make_counted<default_scheduled_actor>(std::move(fun));
}

event_based_actor::event_based_actor(actor_state st) : super(st, true) { }

resume_result event_based_actor::resume(util::fiber*, actor_ptr& next_job) {
    CPPA_LOG_TRACE("id = " << id() << ", state = " << static_cast<int>(state()));
    CPPA_REQUIRE(   state() == actor_state::ready
                 || state() == actor_state::pending);
    scoped_self_setter sss{this};
    auto done_cb = [&]() -> bool {
        CPPA_LOG_TRACE("");
        if (exit_reason() == exit_reason::not_exited) {
            if (planned_exit_reason() == exit_reason::not_exited) {
                planned_exit_reason(exit_reason::normal);
            }
            on_exit();
            if (!m_bhvr_stack.empty()) {
                planned_exit_reason(exit_reason::not_exited);
                return false; // on_exit did set a new behavior
            }
            cleanup(planned_exit_reason());
        }
        set_state(actor_state::done);
        m_bhvr_stack.clear();
        m_bhvr_stack.cleanup();
        on_exit();
        CPPA_REQUIRE(next_job == nullptr);
        next_job.swap(m_chained_actor);
        return true;
    };
    CPPA_REQUIRE(next_job == nullptr);
    try {
        //auto e = m_mailbox.try_pop();
        for (auto e = m_mailbox.try_pop(); ; e = m_mailbox.try_pop()) {
            //e = m_mailbox.try_pop();
            if (e == nullptr) {
                CPPA_REQUIRE(next_job == nullptr);
                CPPA_LOGMF(CPPA_DEBUG, self, "no more element in mailbox; going to block");
                next_job.swap(m_chained_actor);
                set_state(actor_state::about_to_block);
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (this->m_mailbox.can_fetch_more() == false) {
                    switch (compare_exchange_state(actor_state::about_to_block,
                                                   actor_state::blocked)) {
                        case actor_state::ready:
                            // interrupted by arriving message
                            // restore members
                            CPPA_REQUIRE(m_chained_actor == nullptr);
                            next_job.swap(m_chained_actor);
                            CPPA_LOGMF(CPPA_DEBUG, self, "switched back to ready: "
                                           "interrupted by arriving message");
                            break;
                        case actor_state::blocked:
                            CPPA_LOGMF(CPPA_DEBUG, self, "set state successfully to blocked");
                            // done setting actor to blocked
                            return resume_result::actor_blocked;
                        default:
                            CPPA_LOGMF(CPPA_ERROR, self, "invalid state");
                            CPPA_CRITICAL("invalid state");
                    };
                }
                else {
                    CPPA_LOGMF(CPPA_DEBUG, self, "switched back to ready: "
                                   "mailbox can fetch more");
                    set_state(actor_state::ready);
                    CPPA_REQUIRE(m_chained_actor == nullptr);
                    next_job.swap(m_chained_actor);
                }
            }
            else {
                CPPA_LOGMF(CPPA_DEBUG, self, "try to invoke message: " << to_string(e->msg));
                if (m_bhvr_stack.invoke(m_recv_policy, this, e)) {
                    CPPA_LOG_DEBUG_IF(m_chained_actor,
                                      "set actor with ID "
                                      << m_chained_actor->id()
                                      << " as successor");
                    if (m_bhvr_stack.empty() && done_cb()) {
                        CPPA_LOGMF(CPPA_DEBUG, self, "behavior stack empty");
                        return resume_result::actor_done;
                    }
                    m_bhvr_stack.cleanup();
                }
            }
        }
    }
    catch (actor_exited& what) {
        CPPA_LOG_INFO("actor died because of exception: actor_exited, "
                      "reason = " << what.reason());
        if (exit_reason() == exit_reason::not_exited) {
            quit(what.reason());
        }
    }
    catch (std::exception& e) {
        CPPA_LOG_WARNING("actor died because of exception: "
                         << detail::demangle(typeid(e))
                         << ", what() = " << e.what());
        if (exit_reason() == exit_reason::not_exited) {
            quit(exit_reason::unhandled_exception);
        }
    }
    catch (...) {
        CPPA_LOG_WARNING("actor died because of an unknown exception");
        if (exit_reason() == exit_reason::not_exited) {
            quit(exit_reason::unhandled_exception);
        }
    }
    done_cb();
    return resume_result::actor_done;
}

scheduled_actor_type event_based_actor::impl_type() {
    return event_based_impl;
}

} // namespace cppa
