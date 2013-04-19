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


#ifndef CPPA_THREADED_HPP
#define CPPA_THREADED_HPP

#include <mutex>
#include <chrono>
#include <condition_variable>

#include "cppa/mailbox_element.hpp"

#include "cppa/util/dptr.hpp"

#include "cppa/detail/sync_request_bouncer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { namespace detail { class receive_policy; } }

namespace cppa {

template<class Base, class Subtype>
class threaded : public Base {

    friend class detail::receive_policy;

    typedef std::unique_lock<std::mutex> lock_type;

 public:

    template<typename... Ts>
    threaded(Ts&&... args) : Base(std::forward<Ts>(args)...) { }

    inline void reset_timeout() { }

    inline void request_timeout(const util::duration&) { }

    inline void handle_timeout(behavior& bhvr) { bhvr.handle_timeout(); }

    inline void pop_timeout() { }

    inline void push_timeout() { }

    inline bool waits_for_timeout(std::uint32_t) { return false; }

    mailbox_element* pop() {
        wait_for_data();
        return this->m_mailbox.try_pop();
    }

    inline mailbox_element* try_pop() {
        return this->m_mailbox.try_pop();
    }

    template<typename TimePoint>
    mailbox_element* try_pop(const TimePoint& abs_time) {
        return (timed_wait_for_data(abs_time)) ? try_pop() : nullptr;
    }

    bool push_back(mailbox_element* new_element) {
        switch (this->m_mailbox.enqueue(new_element)) {
            case intrusive::first_enqueued: {
                lock_type guard(m_mtx);
                m_cv.notify_one();
                return true;
            }
            default: return true;
            case intrusive::queue_closed: return false;
        }
    }

    void run_detached() {
        auto dthis = util::dptr<Subtype>(this);
        dthis->init();
        dthis->m_bhvr_stack.exec(dthis->m_recv_policy, dthis);
        dthis->on_exit();
    }

 protected:

    typedef threaded combined_type;

    virtual void enqueue(const message_header& hdr, any_tuple msg) override {
        auto ptr = this->new_mailbox_element(hdr, std::move(msg));
        if (!push_back(ptr) && hdr.id.valid()) {
            detail::sync_request_bouncer f{this->exit_reason()};
            f(hdr.sender, hdr.id);
        }
    }

    typedef std::chrono::high_resolution_clock::time_point timeout_type;

    timeout_type init_timeout(const util::duration& rel_time) {
        auto result = std::chrono::high_resolution_clock::now();
        result += rel_time;
        return result;
    }

    mailbox_element* await_message() {
        return pop();
    }

    mailbox_element* await_message(const timeout_type& abs_time) {
        return try_pop(abs_time);
    }

    bool timed_wait_for_data(const timeout_type& abs_time) {
        CPPA_REQUIRE(not this->m_mailbox.closed());
        if (this->m_mailbox.empty()) {
            lock_type guard(m_mtx);
            while (this->m_mailbox.empty()) {
                if (m_cv.wait_until(guard, abs_time) == std::cv_status::timeout) {
                    return false;
                }
            }
        }
        return true;
    }

    void wait_for_data() {
        if (this->m_mailbox.empty()) {
            lock_type guard(m_mtx);
            while (this->m_mailbox.empty()) m_cv.wait(guard);
        }
    }

    std::mutex m_mtx;
    std::condition_variable m_cv;

};

} // namespace cppa

#endif // CPPA_THREADED_HPP
