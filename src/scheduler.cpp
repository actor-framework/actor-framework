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
#include <condition_variable>

#include "cppa/on.hpp"
#include "cppa/policy.hpp"
#include "cppa/logging.hpp"
#include "cppa/anything.hpp"
#include "cppa/to_string.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/scoped_actor.hpp"
#include "cppa/system_messages.hpp"

#include "cppa/detail/proper_actor.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"

using std::move;

namespace cppa {

namespace scheduler {

/******************************************************************************
 *                     utility and implementation details                     *
 ******************************************************************************/

namespace {

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

class timer_actor final : public detail::proper_actor<blocking_actor,
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
                    std::cerr << "coordinator::timer_loop: UNKNOWN MESSAGE: "
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

void printer_loop(blocking_actor* self) {
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
            if (!str.empty() && s) {
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
        on_arg_match >> [&](const down_msg& dm) {
            flush_output(dm.source);
            out.erase(dm.source);
        },
        on(atom("DIE")) >> [&] {
            running = false;
        },
        others() >> [self] {
            std::cerr << "*** unexpected: "
                      << to_string(self->last_dequeued())
                      << std::endl;
        }
    );
}

} // namespace <anonymous>

/******************************************************************************
 *                      implementation of coordinator                         *
 ******************************************************************************/

class coordinator::shutdown_helper : public resumable {

 public:

    void attach_to_scheduler() override { }

    void detach_from_scheduler() override { }

    resumable::resume_result resume(detail::cs_thread*, execution_unit* ptr) {
        auto w = dynamic_cast<worker*>(ptr);
        CPPA_REQUIRE(w != nullptr);
        w->m_running = false;
        std::unique_lock<std::mutex> guard(mtx);
        last_worker = w;
        cv.notify_all();
        return resumable::resume_later;
    }

    shutdown_helper() : last_worker(nullptr) { }

    std::mutex mtx;
    std::condition_variable cv;
    worker* last_worker;

};

void coordinator::initialize() {
    // launch threads of utility actors
    auto ptr = m_timer.get();
    m_timer_thread = std::thread{[ptr] {
        ptr->act();
    }};
    m_printer_thread = std::thread{printer_loop, m_printer.get()};
    // create workers
    size_t hc = std::thread::hardware_concurrency();
    for (size_t i = 0; i < hc; ++i) {
        m_workers.emplace_back(new worker(i, this));
    }
    // start all workers
    for (auto& w : m_workers) w->start();
}

void coordinator::destroy() {
    // shutdown workers
    shutdown_helper sh;
    std::vector<worker*> alive_workers;
    for (auto& w : m_workers) alive_workers.push_back(w.get());
    while (!alive_workers.empty()) {
        alive_workers.back()->external_enqueue(&sh);
        // since jobs can be stolen, we cannot assume that we have
        // actually shut down the worker we've enqueued sh to
        { // lifetime scope of guard
            std::unique_lock<std::mutex> guard(sh.mtx);
            sh.cv.wait(guard, [&]{ return sh.last_worker != nullptr; });
        }
        auto first = alive_workers.begin();
        auto last = alive_workers.end();
        auto i = std::find(first, last, sh.last_worker);
        sh.last_worker = nullptr;
        alive_workers.erase(i);
    }
    // shutdown utility actors
    auto msg = make_any_tuple(atom("DIE"));
    m_timer->enqueue({invalid_actor_addr, nullptr}, msg, nullptr);
    m_printer->enqueue({invalid_actor_addr, nullptr}, msg, nullptr);
    m_timer_thread.join();
    m_printer_thread.join();
    // join each worker thread for good manners
    for (auto& w : m_workers) w->m_this_thread.join();
    // cleanup
    delete this;
}

coordinator::coordinator()
        : m_timer(new timer_actor), m_printer(true)
        , m_next_worker(0) {
    // NOP
}

coordinator* coordinator::create_singleton() {
    return new coordinator;
}

actor coordinator::printer() const {
    return m_printer.get();
}

void coordinator::enqueue(resumable* what) {
    size_t nw = m_next_worker++;
    m_workers[nw % m_workers.size()]->external_enqueue(what);
}

/******************************************************************************
 *                          implementation of worker                          *
 ******************************************************************************/

worker::worker(size_t id, coordinator* parent)
        : m_running(false), m_id(id), m_last_victim(id), m_parent(parent) { }


void worker::start() {
    auto this_worker = this;
    m_this_thread = std::thread{[this_worker] {
        this_worker->run();
    }};
}

void worker::run() {
    CPPA_LOG_TRACE(CPPA_ARG(m_id));
    // local variables
    detail::cs_thread fself;
    job_ptr job = nullptr;
    // some utility functions
    auto local_poll = [&]() -> bool {
        if (!m_job_list.empty()) {
            job = m_job_list.back();
            m_job_list.pop_back();
            return true;
        }
        return false;
    };
    auto aggressive_poll = [&]() -> bool {
        for (int i = 1; i < 101; ++i) {
            job = m_exposed_queue.try_pop();
            if (job) return true;
            // try to steal every 10 poll attempts
            if ((i % 10) == 0) {
                job = raid();
                if (job) return true;
            }
            std::this_thread::yield();
        }
        return false;
    };
    auto moderate_poll = [&]() -> bool {
        for (int i = 1; i < 550; ++i) {
            job =  m_exposed_queue.try_pop();
            if (job) return true;
            // try to steal every 5 poll attempts
            if ((i % 5) == 0) {
                job = raid();
                if (job) return true;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        return false;
    };
    auto relaxed_poll = [&]() -> bool {
        for (;;) {
            job =  m_exposed_queue.try_pop();
            if (job) return true;
            // always try to steal at this stage
            job = raid();
            if (job) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    // scheduling loop
    m_running = true;
    while (m_running) {
        local_poll() || aggressive_poll() || moderate_poll() || relaxed_poll();
        CPPA_LOG_DEBUG("dequeued new job");
        if (job->resume(&fself, this) == resumable::done) {
            // was attached in policy::cooperative_scheduling::launch
            job->detach_from_scheduler();
        }
        job = nullptr;
    }
}

worker::job_ptr worker::try_steal() {
    return m_exposed_queue.try_pop();
}

worker::job_ptr worker::raid() {
    // try once to steal from anyone
    auto n = m_parent->num_workers();
    for (size_t i = 0; i < n; ++i) {
        m_last_victim = (m_last_victim + 1) % n;
        if (m_last_victim != m_id) {
            auto job = m_parent->worker_by_id(m_last_victim)->try_steal();
            if (job) return job;
        }
    }
    return nullptr;
}

void worker::external_enqueue(job_ptr ptr) {
    m_exposed_queue.push_back(ptr);
}

void worker::exec_later(job_ptr ptr) {
    m_job_list.push_back(ptr);
}

} // namespace scheduler

} // namespace cppa
