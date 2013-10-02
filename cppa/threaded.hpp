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

    typedef std::chrono::high_resolution_clock::time_point timeout_type;

    template<typename... Ts>
    threaded(Ts&&... args) : Base(std::forward<Ts>(args)...), m_initialized(false) { }

    inline void reset_timeout() { }

    inline void request_timeout(const util::duration&) { }

    inline void handle_timeout(behavior& bhvr) { bhvr.handle_timeout(); }

    inline void pop_timeout() { }

    inline void push_timeout() { }

    inline bool waits_for_timeout(std::uint32_t) { return false; }

    virtual mailbox_element* try_pop() {
        return this->m_mailbox.try_pop();
    }

    mailbox_element* pop() {
        wait_for_data();
        return try_pop();
    }

    inline mailbox_element* try_pop(const timeout_type& abs_time) {
        return (timed_wait_for_data(abs_time)) ? try_pop() : nullptr;
    }

    void run_detached() {
        auto dthis = util::dptr<Subtype>(this);
        dthis->init();
        dthis->m_bhvr_stack.exec(dthis->m_recv_policy, dthis);
        dthis->on_exit();
    }

    inline void initialized(bool value) {
        m_initialized = value;
    }

    virtual bool initialized() const override {
        return m_initialized;
    }

 protected:

    typedef threaded combined_type;

    void enqueue_impl(typename Base::mailbox_type& mbox,
                      const message_header& hdr,
                      any_tuple&& msg) {
        auto ptr = this->new_mailbox_element(hdr, std::move(msg));
        switch (mbox.enqueue(ptr)) {
            case intrusive::first_enqueued: {
                lock_type guard(m_mtx);
                m_cv.notify_one();
                break;
            }
            default: break;
            case intrusive::queue_closed:
                if (hdr.id.valid()) {
                    detail::sync_request_bouncer f{this->exit_reason()};
                    f(hdr.sender, hdr.id);
                }
                break;
        }
    }

    virtual void enqueue(const message_header& hdr, any_tuple msg) override {
        enqueue_impl(this->m_mailbox, hdr, std::move(msg));
    }

    virtual bool chained_enqueue(const message_header& hdr, any_tuple msg) override {
        enqueue(hdr, std::move(msg));
        return false;
    }

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

    virtual bool mailbox_empty() {
        return this->m_mailbox.empty();
    }

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

 private:

    bool m_initialized;

};

} // namespace cppa

#endif // CPPA_THREADED_HPP
