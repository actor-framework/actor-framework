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
#include "cppa/logging.hpp"
#include "cppa/anything.hpp"
#include "cppa/to_string.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/thread_mapped_actor.hpp"
#include "cppa/context_switching_actor.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

using std::move;

namespace cppa { namespace {

typedef std::uint32_t ui32;

typedef std::chrono::high_resolution_clock hrc;

struct exit_observer : cppa::attachable {
    ~exit_observer() { get_actor_registry()->dec_running(); }
    void actor_exited(std::uint32_t) { }
    bool matches(const token&) { return false; }
};

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

} // namespace <anonymous>

class scheduler_helper {

 public:

    typedef intrusive_ptr<thread_mapped_actor> ptr_type;

    void start() {
        auto mt = make_counted<thread_mapped_actor>();
        auto mp = make_counted<thread_mapped_actor>();
        // launch threads
        m_timer_thread = std::thread{&scheduler_helper::timer_loop, mt.get()};
        m_printer_thread = std::thread{&scheduler_helper::printer_loop, mp.get()};
        // set member variables
        m_timer = std::move(mt);
        m_printer = std::move(mp);
    }

    void stop() {
        auto msg = make_any_tuple(atom("DIE"));
        m_timer.enqueue({invalid_actor_addr, nullptr}, msg);
        m_printer.enqueue({invalid_actor_addr, nullptr}, msg);
        m_timer_thread.join();
        m_printer_thread.join();
    }

    actor m_timer;
    std::thread m_timer_thread;

    actor m_printer;
    std::thread m_printer_thread;

 private:

    static void timer_loop(thread_mapped_actor* self);

    static void printer_loop(thread_mapped_actor* self);

};

template<class Map>
inline void insert_dmsg(Map& storage,
                        const util::duration& d,
                        message_header&& hdr,
                        any_tuple&& tup        ) {
    auto tout = hrc::now();
    tout += d;
    delayed_msg dmsg{move(hdr), move(tup)};
    storage.insert(std::make_pair(std::move(tout), std::move(dmsg)));
}

void scheduler_helper::timer_loop(thread_mapped_actor* self) {
    // setup & local variables
    bool done = false;
    std::unique_ptr<mailbox_element, detail::disposer> msg_ptr;
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
#           ifdef CPPA_DEBUG_MODE
                std::cerr << "scheduler_helper::timer_loop: UNKNOWN MESSAGE: "
                          << to_string(msg_ptr->msg)
                          << std::endl;
#           endif
        }
    );
    // loop
    while (!done) {
        while (!msg_ptr) {
            if (messages.empty()) msg_ptr.reset(self->pop());
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
                    msg_ptr.reset(self->try_pop(it->first));
                }
            }
        }
        mfun(msg_ptr->msg);
        msg_ptr.reset();
    }
}

void scheduler_helper::printer_loop(thread_mapped_actor* self) {
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

actor& scheduler::delayed_send_helper() {
    return m_helper->m_timer;
}

void scheduler::register_converted_context(abstract_actor* what) {
    if (what) {
        get_actor_registry()->inc_running();
        what->address()->attach(attachable_ptr{new exit_observer});
    }
}

attachable* scheduler::register_hidden_context() {
    get_actor_registry()->inc_running();
    return new exit_observer;
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

const actor& scheduler::printer() const {
    return m_helper->m_printer;
}

} // namespace cppa
