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
#include <condition_variable>

#include "cppa/detail/sync_request_bouncer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { namespace policy {

class no_scheduling {

    typedef std::unique_lock<std::mutex> lock_type;

 public:

    template<class Actor>
    void enqueue(Actor* self, const message_header& hdr, any_tuple& msg) {
        auto ptr = self->new_mailbox_element(hdr, std::move(msg));
        switch (self->mailbox().enqueue(ptr)) {
            case intrusive::first_enqueued: {
                lock_type guard(m_mtx);
                m_cv.notify_one();
                break;
            }
            default: break;
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
        std::thread([=] {
            auto rr = resume_result::actor_blocked;
            while (rr != resume_result::actor_done) {
                wait_for_data();
                self->resume();
            }

        }).detach();
    }

 private:

    bool timed_wait_for_data(const timeout_type& abs_time) {
        CPPA_REQUIRE(not this->m_mailbox.closed());
        if (mailbox_empty()) {
            lock_type guard(m_mtx);
            while (mailbox_empty()) {
                if (m_cv.wait_until(guard, abs_time) == std::cv_status::timeout) {
                    return false;
                }
            }
        }
        return true;
    }

    void wait_for_data() {
        if (mailbox_empty()) {
            lock_type guard(m_mtx);
            while (mailbox_empty()) m_cv.wait(guard);
        }
    }

    std::mutex m_mtx;
    std::condition_variable m_cv;

};

} } // namespace cppa::policy

#endif // CPPA_NO_SCHEDULING_HPP
