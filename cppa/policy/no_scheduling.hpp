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


#ifndef CPPA_NO_SCHEDULING_HPP
#define CPPA_NO_SCHEDULING_HPP

#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>

#include "cppa/logging.hpp"
#include "cppa/singletons.hpp"
#include "cppa/actor_state.hpp"
#include "cppa/exit_reason.hpp"

#include "cppa/util/fiber.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/util/scope_guard.hpp"

#include "cppa/policy/scheduling_policy.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { namespace policy {

class no_scheduling {

    typedef std::unique_lock<std::mutex> lock_type;

 public:

    typedef std::chrono::high_resolution_clock::time_point timeout_type;

    template<class Actor, typename F>
    bool fetch_messages(Actor* self, F cb) {
        await_data(self);
        return fetch_messages_impl(self, cb);
    }

    template<class Actor, typename F>
    bool try_fetch_messages(Actor* self, F cb) {
        return fetch_messages_impl(self, cb);
    }

    template<class Actor, typename F>
    timed_fetch_result fetch_messages(Actor* self, F cb, timeout_type abs_time) {
        if (!await_data(self, abs_time)) {
            return timed_fetch_result::no_message;
        }
        if (fetch_messages_impl(self, cb)) return timed_fetch_result::success;
        return timed_fetch_result::no_message;
    }

    template<class Actor>
    void enqueue(Actor* self, const message_header& hdr, any_tuple& msg) {
        auto ptr = self->new_mailbox_element(hdr, std::move(msg));
        switch (self->mailbox().enqueue(ptr)) {
            default:
                break;
            case intrusive::first_enqueued: {
                lock_type guard(m_mtx);
                self->set_state(actor_state::ready);
                m_cv.notify_one();
                break;
            }
            case intrusive::queue_closed:
                if (hdr.id.valid()) {
                    detail::sync_request_bouncer f{self->exit_reason()};
                    f(hdr.sender, hdr.id);
                }
                break;
        }
    }

    template<class Actor>
    void launch(Actor* self) {
        CPPA_PUSH_AID(self->id());
        CPPA_LOG_TRACE(CPPA_ARG(self));
        CPPA_REQUIRE(self != nullptr);
        get_actor_registry()->inc_running();
        intrusive_ptr<Actor> mself{self};
        std::thread([=] {
            CPPA_PUSH_AID(mself->id());
            CPPA_LOG_TRACE("");
            auto guard = util::make_scope_guard([] {
                get_actor_registry()->dec_running();
            });
            util::fiber fself;
            for (;;) {
                mself->set_state(actor_state::ready);
                if (mself->resume(&fself) == resumable::done) {
                    return;
                }
                // await new data before resuming actor
                await_data(mself.get());
            }
        }).detach();
    }

    template<class Actor>
    void await_data(Actor* self) {
        if (!self->has_next_message()) {
            lock_type guard(m_mtx);
            while (!self->has_next_message()) m_cv.wait(guard);
        }
    }

    // this additional member function is needed to implement
    // timer_actor (see scheduler.cpp)
    template<class Actor, class TimePoint>
    bool await_data(Actor* self, const TimePoint& tp) {
        if (!self->has_next_message()) {
            lock_type guard(m_mtx);
            while (!self->has_next_message()) {
                if (m_cv.wait_until(guard, tp) == std::cv_status::timeout) {
                    return false;
                }
            }
        }
        return true;
    }

 private:

    template<class Actor, typename F>
    bool fetch_messages_impl(Actor* self, F cb) {
        auto next = [&] { return self->mailbox().try_pop(); };
        auto e = next();
        if (e) {
            for (; e != nullptr; e = next()) {
                cb(e);
            }
            return true;
        }
        return false;
    }

    std::mutex m_mtx;
    std::condition_variable m_cv;

};

} } // namespace cppa::policy

#endif // CPPA_NO_SCHEDULING_HPP
