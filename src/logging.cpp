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


#include <ctime>
#include <thread>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <pthread.h>

#ifndef CPPA_WINDOWS
#include <unistd.h>
#include <sys/types.h>
#endif

#include "cppa/cppa.hpp"
#include "cppa/logging.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/intrusive/blocking_single_reader_queue.hpp"

using namespace std;

namespace cppa {

namespace {

__thread actor_id t_self_id;

template<size_t RawSize>
void replace_all(string& str, const char (&before)[RawSize], const char* after) {
    // end(before) - 1 points to the null-terminator
    auto i = search(begin(str), end(str), begin(before), end(before) - 1);
    while (i != end(str)) {
        str.replace(i, i + RawSize - 1, after);
        i = search(begin(str), end(str), begin(before), end(before) - 1);
    }
}

constexpr struct pop_aid_log_event_t { constexpr pop_aid_log_event_t() { } }
          pop_aid_log_event;

struct log_event {
    log_event* next;
    string msg;
};

class logging_impl : public logging {

 public:

    void initialize() {
        m_thread = thread([this] { (*this)(); });
        log("TRACE", "logging", "run", __FILE__, __LINE__, "ENTRY");
    }

    void destroy() {
        log("TRACE", "logging", "run", __FILE__, __LINE__, "EXIT");
        // an empty string means: shut down
        m_queue.push_back(new log_event{0, ""});
        m_thread.join();
        delete this;
    }

    void operator()() {
        ostringstream fname;
        fname << "libcppa_" << getpid() << "_" << time(0) << ".log";
        fstream out(fname.str().c_str(), ios::out | ios::app);
        unique_ptr<log_event> event;
        for (;;) {
            event.reset(m_queue.pop());
            if (event->msg.empty()) {
                out.close();
                return;
            }
            else out << event->msg << flush;
        }
    }

    actor_id set_aid(actor_id aid) override {
        actor_id prev = t_self_id;
        t_self_id = aid;
        return prev;
    }

    void log(const char* level,
             const char* c_class_name,
             const char* function_name,
             const char* c_full_file_name,
             int line_num,
             const std::string& msg) override {
        string class_name = c_class_name;
        replace_all(class_name, "::", ".");
        replace_all(class_name, "(anonymous namespace)", "$anon$");
        string file_name;
        string full_file_name = c_full_file_name;
        auto ri = find(full_file_name.rbegin(), full_file_name.rend(), '/');
        if (ri != full_file_name.rend()) {
            auto i = ri.base();
            if (i == full_file_name.end()) file_name = move(full_file_name);
            else file_name = string(i, full_file_name.end());
        }
        else file_name = move(full_file_name);
        ostringstream line;
        line << time(0) << " "
             << level << " "
             << "actor" << t_self_id << " "
             << this_thread::get_id() << " "
             << class_name << " "
             << function_name << " "
             << file_name << ":" << line_num << " "
             << msg
             << endl;
        m_queue.push_back(new log_event{nullptr, line.str()});
    }

 private:

    thread m_thread;
    intrusive::blocking_single_reader_queue<log_event> m_queue;

};

} // namespace <anonymous>

logging::trace_helper::trace_helper(std::string class_name,
                                    const char* fun_name,
                                    const char* file_name,
                                    int line_num,
                                    const std::string& msg)
: m_class(std::move(class_name)), m_fun_name(fun_name)
, m_file_name(file_name), m_line_num(line_num) {
    get_logger()->log("TRACE", m_class.c_str(), fun_name,
                      file_name, line_num, "ENTRY " + msg);
}

logging::trace_helper::~trace_helper() {
    get_logger()->log("TRACE", m_class.c_str(), m_fun_name,
                      m_file_name, m_line_num, "EXIT");
}

logging::~logging() { }

logging* logging::create_singleton() { return new logging_impl; }

} // namespace cppa
