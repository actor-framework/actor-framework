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
    template<class Base, class Derived>
    struct mixin : Base, resumable {

        template<typename... Ts>
        mixin(Ts&&... args) : Base(std::forward<Ts>(args)...) { }

        inline Derived* dptr() {
            return static_cast<Derived*>(this);
        }

        resumable::resume_result resume(util::fiber*) override {
            auto d = dptr();
            CPPA_LOG_TRACE("id = " << d->id()
                           << ", state = " << static_cast<int>(d->state()));
            CPPA_REQUIRE(   d->state() == actor_state::ready
                         || d->state() == actor_state::pending);
            CPPA_PUSH_AID(d->id());
            auto done_cb = [&]() -> bool {
                CPPA_LOG_TRACE("");
                if (   d->exit_reason() == exit_reason::not_exited
                    && d->planned_exit_reason() == exit_reason::not_exited) {
                    d->planned_exit_reason(exit_reason::normal);
                }
                d->on_exit();
                if (!d->bhvr_stack().empty()) {
                    d->planned_exit_reason(exit_reason::not_exited);
                    return false; // on_exit did set a new behavior
                }
                d->set_state(actor_state::done);
                d->bhvr_stack().clear();
                d->bhvr_stack().cleanup();
                d->cleanup(d->planned_exit_reason());
                return true;
            };
            try {
                for (;;) {
                    auto ptr = dptr()->next_message();
                    if (ptr) {
                        CPPA_REQUIRE(!dptr()->bhvr_stack().empty());
                        auto bhvr = dptr()->bhvr_stack().back();
                        auto mid = dptr()->bhvr_stack().back_id();
                        if (dptr()->invoke_message(ptr, bhvr, mid)) {
                            if (dptr()->bhvr_stack().empty() && done_cb()) {
                                CPPA_LOG_DEBUG("behavior stack empty");
                                return resume_result::done;
                            }
                        }
                        // add ptr to cache if invoke_message did not
                        // reset it
                        if (ptr) dptr()->push_to_cache(std::move(ptr));
                    }
                    else {
                        CPPA_LOG_DEBUG("no more element in mailbox; going to block");
                        d->set_state(actor_state::about_to_block);
                        std::atomic_thread_fence(std::memory_order_seq_cst);
                        if (!dptr()->has_next_message()) {
                            switch (d->cas_state(actor_state::about_to_block,
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
                            d->set_state(actor_state::ready);
                        }
                    }
                }
            }
            catch (actor_exited& what) {
                CPPA_LOG_INFO("actor died because of exception: actor_exited, "
                              "reason = " << what.reason());
                if (d->exit_reason() == exit_reason::not_exited) {
                    d->quit(what.reason());
                }
            }
            catch (std::exception& e) {
                CPPA_LOG_WARNING("actor died because of exception: "
                                 << detail::demangle(typeid(e))
                                 << ", what() = " << e.what());
                if (d->exit_reason() == exit_reason::not_exited) {
                    d->quit(exit_reason::unhandled_exception);
                }
            }
            catch (...) {
                CPPA_LOG_WARNING("actor died because of an unknown exception");
                if (d->exit_reason() == exit_reason::not_exited) {
                    d->quit(exit_reason::unhandled_exception);
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
        return false;
    }

};

} } // namespace cppa::policy

#endif // CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP
