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

#include "cppa/util/fiber.hpp"
#include "cppa/util/duration.hpp"

#include "cppa/policy/scheduling_policy.hpp"

#include "cppa/detail/sync_request_bouncer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { namespace policy {

class no_scheduling {

    typedef std::unique_lock<std::mutex> lock_type;

 public:

    typedef std::chrono::high_resolution_clock::time_point timeout_type;

    template<class Actor>
    inline timeout_type init_timeout(Actor*, const util::duration& rel_time) {
        auto result = std::chrono::high_resolution_clock::now();
        result += rel_time;
        return result;
    }

    template<class Actor, typename F>
    bool fetch_messages(Actor* self, F cb) {
        await_data(self);
        fetch_messages_impl(self, cb);
    }

    template<class Actor, typename F>
    bool try_fetch_messages(Actor* self, F cb) {
        auto next = [&] { return self->mailbox().try_pop(); };
        auto e = next();
        if (!e) return false;
        do {
            cb(e);
            e = next();
        }
        while (e);
        return true;
    }

    template<class Actor, typename F>
    timed_fetch_result fetch_messages(Actor* self, F cb, timeout_type abs_time) {
        if (!await_data(self, abs_time)) {
            return timed_fetch_result::no_message;
        }
        fetch_messages_impl(self, cb);
        return timed_fetch_result::success;
    }

    template<class Actor>
    void enqueue(Actor* self, const message_header& hdr, any_tuple& msg) {
std::cout << "enqueue\n";
        auto ptr = self->new_mailbox_element(hdr, std::move(msg));
        switch (self->mailbox().enqueue(ptr)) {
            default:
std::cout << "enqueue: default case (do nothing)\n";
                break;
            case intrusive::first_enqueued: {
std::cout << "enqueue: first enqueue -> notify\n";
                lock_type guard(m_mtx);
                m_cv.notify_one();
                break;
            }
            case intrusive::queue_closed:
std::cout << "enqueue: mailbox closed!\n";
                if (hdr.id.valid()) {
                    detail::sync_request_bouncer f{self->exit_reason()};
                    f(hdr.sender, hdr.id);
                }
                break;
        }
    }

    template<class Actor>
    void launch(Actor* self) {
        std::thread([=] {
            util::fiber fself;
            auto rr = resumable::resume_later;
            while (rr != resumable::done) {
std::cout << "before await_data\n";
                await_data(self);
std::cout << "before resume\n";
                rr = self->resume(&fself);
std::cout << "after resume\n";
            }
        }).detach();
    }

    template<class Actor>
    void await_data(Actor* self) {
        while (self->mailbox().empty()) {
            lock_type guard(m_mtx);
            while (self->mailbox().empty()) m_cv.wait(guard);
        }
    }

    template<class Actor>
    bool await_data(Actor* self, const timeout_type& abs_time) {
        CPPA_REQUIRE(!self->mailbox().closed());
        while (self->mailbox().empty()) {
            lock_type guard(m_mtx);
            while (self->mailbox().empty()) {
                if (m_cv.wait_until(guard, abs_time) == std::cv_status::timeout) {
                    return false;
                }
            }
        }
        return true;
    }

 private:

    template<class Actor, typename F>
    void fetch_messages_impl(Actor* self, F cb) {
        auto next = [&] { return self->mailbox().try_pop(); };
        for (auto e = next(); e != nullptr; e = next()) {
            cb(e);
        }

    }

    std::mutex m_mtx;
    std::condition_variable m_cv;

};

} } // namespace cppa::policy

#endif // CPPA_NO_SCHEDULING_HPP
