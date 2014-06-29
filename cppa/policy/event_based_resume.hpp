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

#ifndef CPPA_POLICY_ABSTRACT_EVENT_BASED_ACTOR_HPP
#define CPPA_POLICY_ABSTRACT_EVENT_BASED_ACTOR_HPP

#include <tuple>
#include <stack>
#include <memory>
#include <vector>
#include <type_traits>

#include "cppa/config.hpp"
#include "cppa/extend.hpp"
#include "cppa/behavior.hpp"
#include "cppa/scheduler.hpp"

#include "cppa/policy/resume_policy.hpp"

#include "cppa/detail/logging.hpp"

namespace cppa {
namespace policy {

class event_based_resume {

 public:

    // Base must be a mailbox-based actor
    template<class Base, class Derived>
    struct mixin : Base, resumable {

        template<typename... Ts>
        mixin(Ts&&... args)
                : Base(std::forward<Ts>(args)...) {}

        void attach_to_scheduler() override { this->ref(); }

        void detach_from_scheduler() override { this->deref(); }

        resumable::resume_result resume(execution_unit* host) override {
            auto d = static_cast<Derived*>(this);
            d->m_host = host;
            CPPA_LOG_TRACE("id = " << d->id());
            auto done_cb = [&]()->bool {
                CPPA_LOG_TRACE("");
                d->bhvr_stack().clear();
                d->bhvr_stack().cleanup();
                d->on_exit();
                if (!d->bhvr_stack().empty()) {
                    CPPA_LOG_DEBUG("on_exit did set a new behavior in on_exit");
                    d->planned_exit_reason(exit_reason::not_exited);
                    return false; // on_exit did set a new behavior
                }
                auto rsn = d->planned_exit_reason();
                if (rsn == exit_reason::not_exited) {
                    rsn = exit_reason::normal;
                    d->planned_exit_reason(rsn);
                }
                d->cleanup(rsn);
                return true;

            };
            auto actor_done = [&] {
                return d->bhvr_stack().empty() ||
                       d->planned_exit_reason() != exit_reason::not_exited;

            };
            // actors without behavior or that have already defined
            // an exit reason must not be resumed
            CPPA_REQUIRE(!d->m_initialized || !actor_done());
            if (!d->m_initialized) {
                d->m_initialized = true;
                auto bhvr = d->make_behavior();
                if (bhvr) d->become(std::move(bhvr));
                // else: make_behavior() might have just called become()
                if (actor_done() && done_cb()) return resume_result::done;
                // else: enter resume loop
            }
            try {
                for (;;) {
                    auto ptr = d->next_message();
                    if (ptr) {
                        if (d->invoke_message(ptr)) {
                            if (actor_done() && done_cb()) {
                                CPPA_LOG_DEBUG("actor exited");
                                return resume_result::done;
                            }
                            // continue from cache if current message was
                            // handled, because the actor might have changed
                            // its behavior to match 'old' messages now
                            while (d->invoke_message_from_cache()) {
                                if (actor_done() && done_cb()) {
                                    CPPA_LOG_DEBUG("actor exited");
                                    return resume_result::done;
                                }
                            }
                        }
                        // add ptr to cache if invoke_message
                        // did not reset it (i.e. skipped, but not dropped)
                        if (ptr) {
                            CPPA_LOG_DEBUG("add message to cache");
                            d->push_to_cache(std::move(ptr));
                        }
                    } else {
                        CPPA_LOG_DEBUG(
                            "no more element in mailbox; "
                            "going to block");
                        if (d->mailbox().try_block()) {
                            return resumable::resume_later;
                        }
                        // else: try again
                    }
                }
            }
            catch (actor_exited& what) {
                CPPA_LOG_INFO(
                    "actor died because of exception: actor_exited, "
                    "reason = "
                    << what.reason());
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
    bool await_data(Actor*, const duration&) {
        static_assert(std::is_same<Actor, Actor>::value == false,
                      "The event-based resume policy cannot be used "
                      "to implement blocking actors");
        return false;
    }

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_ABSTRACT_EVENT_BASED_ACTOR_HPP
