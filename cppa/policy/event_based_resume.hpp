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


#ifndef CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP
#define CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP

#include <tuple>
#include <stack>
#include <memory>
#include <vector>
#include <type_traits>

#include "cppa/config.hpp"
#include "cppa/extend.hpp"
#include "cppa/behavior.hpp"
#include "cppa/actor_state.hpp"

#include "cppa/policy/resume_policy.hpp"

namespace cppa { namespace policy {

class event_based_resume {

 public:

    // Base must be a mailbox-based actor
    template<class Base>
    struct mixin : Base, resumable {

        template<typename... Ts>
        mixin(Ts&&... args) : Base(std::forward<Ts>(args)...) { }

        // implemented in detail::proper_actor
        virtual bool invoke(mailbox_element* msg) = 0;

        resumable::resume_result resume(util::fiber*) override {
            CPPA_LOG_TRACE("id = " << this->id()
                           << ", state = " << static_cast<int>(this->state()));
            CPPA_REQUIRE(   this->state() == actor_state::ready
                         || this->state() == actor_state::pending);
            CPPA_PUSH_AID(this->id());
            auto done_cb = [&]() -> bool {
                CPPA_LOG_TRACE("");
                if (this->exit_reason() == exit_reason::not_exited) {
                    if (this->planned_exit_reason() == exit_reason::not_exited) {
                        this->planned_exit_reason(exit_reason::normal);
                    }
                    this->on_exit();
                    if (!this->bhvr_stack().empty()) {
                        this->planned_exit_reason(exit_reason::not_exited);
                        return false; // on_exit did set a new behavior
                    }
                    this->cleanup(this->planned_exit_reason());
                }
                this->set_state(actor_state::done);
                this->bhvr_stack().clear();
                this->bhvr_stack().cleanup();
                this->on_exit();
                return true;
            };
            try {
                //auto e = m_mailbox.try_pop();
                for (auto e = this->m_mailbox.try_pop(); ; e = this->m_mailbox.try_pop()) {
                    //e = m_mailbox.try_pop();
                    if (e == nullptr) {
                        CPPA_LOG_DEBUG("no more element in mailbox; going to block");
                        this->set_state(actor_state::about_to_block);
                        std::atomic_thread_fence(std::memory_order_seq_cst);
                        if (this->m_mailbox.can_fetch_more() == false) {
                            switch (this->cas_state(actor_state::about_to_block,
                                                    actor_state::blocked)) {
                                case actor_state::ready:
                                    // interrupted by arriving message
                                    // restore members
                                    CPPA_LOG_DEBUG("switched back to ready: "
                                                   "interrupted by "
                                                   "arriving message");
                                    break;
                                case actor_state::blocked:
                                    CPPA_LOG_DEBUG("set state successfully to blocked");
                                    // done setting actor to blocked
                                    return resumable::resume_later;
                                default:
                                    CPPA_LOG_ERROR("invalid state");
                                    CPPA_CRITICAL("invalid state");
                            };
                        }
                        else {
                            CPPA_LOG_DEBUG("switched back to ready: "
                                           "mailbox can fetch more");
                            this->set_state(actor_state::ready);
                        }
                    }
                    else {
                        if (this->invoke(e)) {
                            if (this->bhvr_stack().empty() && done_cb()) {
                                CPPA_LOG_DEBUG("behavior stack empty");
                                return resume_result::done;
                            }
                            this->bhvr_stack().cleanup();
                        }
                    }
                }
            }
            catch (actor_exited& what) {
                CPPA_LOG_INFO("actor died because of exception: actor_exited, "
                              "reason = " << what.reason());
                if (this->exit_reason() == exit_reason::not_exited) {
                    this->quit(what.reason());
                }
            }
            catch (std::exception& e) {
                CPPA_LOG_WARNING("actor died because of exception: "
                                 << detail::demangle(typeid(e))
                                 << ", what() = " << e.what());
                if (this->exit_reason() == exit_reason::not_exited) {
                    this->quit(exit_reason::unhandled_exception);
                }
            }
            catch (...) {
                CPPA_LOG_WARNING("actor died because of an unknown exception");
                if (this->exit_reason() == exit_reason::not_exited) {
                    this->quit(exit_reason::unhandled_exception);
                }
            }
            done_cb();
            return resumable::done;
        }

    };

    template<class Actor>
    void await_data(Actor*) {
        static_assert(std::is_same<Actor, Actor>::value == false,
                      "The event-based resume policy cannot be used "
                      "to implement blocking actors");
    }

    template<class Actor>
    bool await_data(Actor*, const util::duration&) {
        static_assert(std::is_same<Actor, Actor>::value == false,
                      "The event-based resume policy cannot be used "
                      "to implement blocking actors");
    }

};

} } // namespace cppa::policy

#endif // CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP
