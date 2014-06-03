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


#ifndef CPPA_POLICY_NO_SCHEDULING_HPP
#define CPPA_POLICY_NO_SCHEDULING_HPP

#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>

#include "cppa/logging.hpp"
#include "cppa/singletons.hpp"
#include "cppa/exit_reason.hpp"

#include "cppa/detail/cs_thread.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/util/scope_guard.hpp"

#include "cppa/policy/scheduling_policy.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa {
namespace policy {

class no_scheduling {

    typedef std::unique_lock<std::mutex> lock_type;

 public:

    typedef std::chrono::high_resolution_clock::time_point timeout_type;

    template<class Actor>
    void enqueue(Actor* self, msg_hdr_cref hdr,
                 any_tuple& msg, execution_unit*) {
        auto ptr = self->new_mailbox_element(hdr, std::move(msg));
        // returns false if mailbox has been closed
        if (!self->mailbox().synchronized_enqueue(m_mtx, m_cv, ptr)) {
            if (hdr.id.is_request()) {
                detail::sync_request_bouncer srb{self->exit_reason()};
                srb(hdr.sender, hdr.id);
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
            detail::cs_thread fself;
            for (;;) {
                if (mself->resume(&fself, nullptr) == resumable::done) {
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

#endif // CPPA_POLICY_NO_SCHEDULING_HPP
