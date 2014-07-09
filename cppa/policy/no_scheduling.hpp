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

#ifndef CPPA_NO_SCHEDULING_HPP
#define CPPA_NO_SCHEDULING_HPP

#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>

#include "cppa/duration.hpp"
#include "cppa/exit_reason.hpp"

#include "cppa/policy/scheduling_policy.hpp"

#include "cppa/detail/logging.hpp"
#include "cppa/detail/singletons.hpp"
#include "cppa/detail/scope_guard.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"
#include "cppa/detail/single_reader_queue.hpp"

namespace cppa {
namespace policy {

class no_scheduling {

    using lock_type = std::unique_lock<std::mutex>;

 public:

    using timeout_type = std::chrono::high_resolution_clock::time_point;

    template<class Actor>
    void enqueue(Actor* self, const actor_addr& sender, message_id mid,
                 message& msg, execution_unit*) {
        auto ptr = self->new_mailbox_element(sender, mid, std::move(msg));
        // returns false if mailbox has been closed
        if (!self->mailbox().synchronized_enqueue(m_mtx, m_cv, ptr)) {
            if (mid.is_request()) {
                detail::sync_request_bouncer srb{self->exit_reason()};
                srb(sender, mid);
            }
        }
    }

    template<class Actor>
    void launch(Actor* self, execution_unit*) {
        CPPA_REQUIRE(self != nullptr);
        CPPA_PUSH_AID(self->id());
        CPPA_LOG_TRACE(CPPA_ARG(self));
        intrusive_ptr<Actor> mself{self};
        self->attach_to_scheduler();
        std::thread([=] {
                        CPPA_PUSH_AID(mself->id());
                        CPPA_LOG_TRACE("");
                        for (;;) {
                            if (mself->resume(nullptr) == resumable::done) {
                                return;
                            }
                            // await new data before resuming actor
                            await_data(mself.get());
                            CPPA_REQUIRE(self->mailbox().blocked() == false);
                        }
                        self->detach_from_scheduler();
                    }).detach();
    }

    // await_data is being called from no_scheduling (only)

    template<class Actor>
    void await_data(Actor* self) {
        if (self->has_next_message()) return;
        self->mailbox().synchronized_await(m_mtx, m_cv);
    }

    // this additional member function is needed to implement
    // timer_actor (see scheduler.cpp)
    template<class Actor, class TimePoint>
    bool await_data(Actor* self, const TimePoint& tp) {
        if (self->has_next_message()) return true;
        return self->mailbox().synchronized_await(m_mtx, m_cv, tp);
    }

 private:

    std::mutex m_mtx;
    std::condition_variable m_cv;

};

} // namespace policy
} // namespace cppa

#endif // CPPA_NO_SCHEDULING_HPP
