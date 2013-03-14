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
 * Free Software Foundation, either version 3 of the License                  *
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
#include "cppa/self.hpp"
#include "cppa/receive.hpp"
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

    delayed_msg(const channel_ptr& arg0,
                const actor_ptr&   arg1,
                message_id,
                any_tuple&&        arg3)
    : ptr_a(arg0), from(arg1), msg(move(arg3)) { }

    delayed_msg(const actor_ptr&   arg0,
                const actor_ptr&   arg1,
                message_id       arg2,
                any_tuple&&        arg3)
    : ptr_b(arg0), from(arg1), id(arg2), msg(move(arg3)) { }

    delayed_msg(delayed_msg&&) = default;
    delayed_msg(const delayed_msg&) = default;
    delayed_msg& operator=(delayed_msg&&) = default;
    delayed_msg& operator=(const delayed_msg&) = default;

    inline void eval() {
        CPPA_REQUIRE(ptr_a || ptr_b);
        if (ptr_a) ptr_a->enqueue(from, move(msg));
        else ptr_b->sync_enqueue(from, id, move(msg));
    }

 private:

    channel_ptr   ptr_a;
    actor_ptr     ptr_b;
    actor_ptr     from;
    message_id  id;
    any_tuple     msg;

};

} // namespace <anonymous>

class scheduler_helper {

 public:

    typedef intrusive_ptr<thread_mapped_actor> ptr_type;

    void start() {
        ptr_type mt{detail::memory::create<thread_mapped_actor>()};
        ptr_type mp{detail::memory::create<thread_mapped_actor>()};
        // launch threads
        m_timer_thread = std::thread{&scheduler_helper::timer_loop, mt};
        m_printer_thread = std::thread{&scheduler_helper::printer_loop, mp};
        // set member variables
        m_timer = std::move(mt);
        m_printer = std::move(mp);
    }

    void stop() {
        auto msg = make_any_tuple(atom("DIE"));
        m_timer->enqueue(nullptr, msg);
        m_printer->enqueue(nullptr, msg);
        m_timer_thread.join();
        m_printer_thread.join();
    }

    actor_ptr m_timer;
    std::thread m_timer_thread;

    actor_ptr m_printer;
    std::thread m_printer_thread;

 private:

    static void timer_loop(ptr_type m_self);

    static void printer_loop(ptr_type m_self);

};

template<class Map, class T>
inline void insert_dmsg(Map& storage,
                 const util::duration& d,
                 const T& to,
                 const actor_ptr& sender,
                 any_tuple&& tup,
                 message_id id = message_id{}) {
    auto tout = hrc::now();
    tout += d;
    delayed_msg dmsg{to, sender, id, move(tup)};
    storage.insert(std::make_pair(std::move(tout), std::move(dmsg)));
}

void scheduler_helper::timer_loop(scheduler_helper::ptr_type m_self) {
    // setup & local variables
    self.set(m_self.get());
    bool done = false;
    std::unique_ptr<mailbox_element,detail::disposer> msg_ptr;
    auto tout = hrc::now();
    std::multimap<decltype(tout),delayed_msg> messages;
    // message handling rules
    auto mfun = (
        on(atom("SEND"), arg_match) >> [&](const util::duration& d,
                                           const channel_ptr& ptr,
                                           any_tuple& tup) {
            insert_dmsg(messages, d, ptr, msg_ptr->sender, move(tup));
        },
        on(atom("REPLY"), arg_match) >> [&](const util::duration& d,
                                            const actor_ptr& ptr,
                                            message_id id,
                                            any_tuple& tup) {
            insert_dmsg(messages, d, ptr, msg_ptr->sender, move(tup), id);
        },
        on(atom("DIE")) >> [&] {
            done = true;
        },
        others() >> [&]() {
#           ifdef CPPA_DEBUG
                std::cerr << "scheduler_helper::timer_loop: UNKNOWN MESSAGE: "
                          << to_string(msg_ptr->msg)
                          << std::endl;
#           endif
        }
    );
    // loop
    while (!done) {
        while (!msg_ptr) {
            if (messages.empty()) msg_ptr.reset(m_self->pop());
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
                    msg_ptr.reset(m_self->try_pop(it->first));
                }
            }
        }
        mfun(msg_ptr->msg);
        msg_ptr.reset();
    }
}

void scheduler_helper::printer_loop(ptr_type m_self) {
    self.set(m_self.get());
    std::map<actor_ptr,std::string> out;
    auto flush_output = [&out](const actor_ptr& s) {
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
    receive_while (gref(running)) (
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

const actor_ptr& scheduler::delayed_send_helper() {
    return m_helper->m_timer;
}

void scheduler::register_converted_context(actor* what) {
    if (what) {
        get_actor_registry()->inc_running();
        what->attach(attachable_ptr{new exit_observer});
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

const actor_ptr& scheduler::printer() const {
    return m_helper->m_printer;
}

} // namespace cppa
