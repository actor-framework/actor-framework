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


#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>

#include "cppa/on.hpp"
#include "cppa/policy.hpp"
#include "cppa/logging.hpp"
#include "cppa/anything.hpp"
#include "cppa/to_string.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/scoped_actor.hpp"

#include "cppa/detail/proper_actor.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

using std::move;

namespace cppa { namespace {

typedef std::uint32_t ui32;

typedef std::chrono::high_resolution_clock hrc;

typedef hrc::time_point time_point;

typedef policy::policies<policy::no_scheduling, policy::not_prioritizing,
                         policy::no_resume, policy::nestable_invoke>
        timer_actor_policies;

class delayed_msg {

 public:

    delayed_msg(message_header&& arg1,
                any_tuple&&      arg2)
    : hdr(move(arg1)), msg(move(arg2)) { }

    delayed_msg(delayed_msg&&) = default;
    delayed_msg(const delayed_msg&) = default;
    delayed_msg& operator=(delayed_msg&&) = default;
    delayed_msg& operator=(const delayed_msg&) = default;

    inline void eval() {
        hdr.deliver(std::move(msg));
    }

 private:

    message_header hdr;
    any_tuple      msg;

};

template<class Map>
inline void insert_dmsg(Map& storage, const util::duration& d,
                        message_header&& hdr, any_tuple&& tup) {
    auto tout = hrc::now();
    tout += d;
    delayed_msg dmsg{move(hdr), move(tup)};
    storage.insert(std::make_pair(std::move(tout), std::move(dmsg)));
}

class timer_actor final : public detail::proper_actor<blocking_untyped_actor,
                                                      timer_actor_policies> {

 public:

    inline unique_mailbox_element_pointer dequeue() {
        await_data();
        return next_message();
    }

    inline unique_mailbox_element_pointer try_dequeue(const time_point& tp) {
        if (scheduling_policy().await_data(this, tp)) {
            return next_message();
        }
        return unique_mailbox_element_pointer{};
    }

    void act() override {
        // setup & local variables
        bool done = false;
        unique_mailbox_element_pointer msg_ptr;
        auto tout = hrc::now();
        std::multimap<decltype(tout), delayed_msg> messages;
        // message handling rules
        auto mfun = (
            on(atom("SEND"), arg_match) >> [&](const util::duration& d,
                                               message_header& hdr,
                                               any_tuple& tup) {
                insert_dmsg(messages, d, move(hdr), move(tup));
            },
            on(atom("DIE")) >> [&] {
                done = true;
            },
            others() >> [&]() {
#               ifdef CPPA_DEBUG_MODE
                    std::cerr << "scheduler_helper::timer_loop: UNKNOWN MESSAGE: "
                              << to_string(msg_ptr->msg)
                              << std::endl;
#               endif
            }
        );
        // loop
        while (!done) {
            while (!msg_ptr) {
                if (messages.empty()) msg_ptr = dequeue();
                else {
                    tout = hrc::now();
                    // handle timeouts (send messages)
                    auto it = messages.begin();
                    while (it != messages.end() && (it->first) <= tout) {
                        it->second.eval();
                        messages.erase(it);
                        it = messages.begin();
                    }
                    // wait for next message or next timeout
                    if (it != messages.end()) {
                        msg_ptr = try_dequeue(it->first);
                    }
                }
            }
            mfun(msg_ptr->msg);
            msg_ptr.reset();
        }
    }

};

} // namespace <anonymous>

class scheduler_helper {

 public:

    scheduler_helper() : m_timer(new timer_actor), m_printer(true) { }

    void start() {
        // launch threads
        m_timer_thread = std::thread{&scheduler_helper::timer_loop, m_timer.get()};
        m_printer_thread = std::thread{&scheduler_helper::printer_loop, m_printer.get()};
    }

    void stop() {
        auto msg = make_any_tuple(atom("DIE"));
        m_timer->enqueue({invalid_actor_addr, nullptr}, msg);
        m_printer->enqueue({invalid_actor_addr, nullptr}, msg);
        m_timer_thread.join();
        m_printer_thread.join();
    }

    intrusive_ptr<timer_actor> m_timer;
    std::thread m_timer_thread;

    scoped_actor m_printer;
    std::thread m_printer_thread;

 private:

    static void timer_loop(timer_actor* self);

    static void printer_loop(blocking_untyped_actor* self);

};

void scheduler_helper::timer_loop(timer_actor* self) {
    self->act();
}

void scheduler_helper::printer_loop(blocking_untyped_actor* self) {
    std::map<actor_addr, std::string> out;
    auto flush_output = [&out](const actor_addr& s) {
        auto i = out.find(s);
        if (i != out.end()) {
            auto& line = i->second;
            if (!line.empty()) {
                std::cout << line << std::flush;
                line.clear();
            }
        }
    };
    auto flush_if_needed = [](std::string& str) {
        if (str.back() == '\n') {
            std::cout << str << std::flush;
            str.clear();
        }
    };
    bool running = true;
    self->receive_while (gref(running)) (
        on(atom("add"), arg_match) >> [&](std::string& str) {
            auto s = self->last_sender();
            if (!str.empty() && s != nullptr) {
                auto i = out.find(s);
                if (i == out.end()) {
                    i = out.insert(make_pair(s, move(str))).first;
                    // monitor actor to flush its output on exit
                    self->monitor(s);
                    flush_if_needed(i->second);
                }
                else {
                    auto& ref = i->second;
                    ref += move(str);
                    flush_if_needed(ref);
                }
            }
        },
        on(atom("flush")) >> [&] {
            flush_output(self->last_sender());
        },
        on(atom("DOWN"), any_vals) >> [&] {
            auto s = self->last_sender();
            flush_output(s);
            out.erase(s);
        },
        on(atom("DIE")) >> [&] {
            running = false;
        },
        others() >> [] {
            //cout << "*** unexpected: "
            //     << to_string(self->last_dequeued())
            //     << endl;
        }
    );
}

scheduler::scheduler() : m_helper(nullptr) { }

void scheduler::initialize() {
    m_helper = new scheduler_helper;
    m_helper->start();
}

void scheduler::destroy() {
    CPPA_LOG_TRACE("");
    m_helper->stop();
    delete this;
}

scheduler::~scheduler() {
    delete m_helper;
}

actor scheduler::delayed_send_helper() {
    return m_helper->m_timer.get();
}

void set_scheduler(scheduler* sched) {
    if (detail::singleton_manager::set_scheduler(sched) == false) {
        throw std::runtime_error("scheduler already set");
    }
}

void set_default_scheduler(size_t num_threads) {
    set_scheduler(new detail::thread_pool_scheduler(num_threads));
}

scheduler* scheduler::create_singleton() {
    return new detail::thread_pool_scheduler;
}

actor scheduler::printer() const {
    return m_helper->m_printer.get();
}

} // namespace cppa
