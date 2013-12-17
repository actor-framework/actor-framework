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

template<size_t RawSize>
void replace_all(string& str, const char (&before)[RawSize], const char* after) {
    // end(before) - 1 points to the null-terminator
    auto i = search(begin(str), end(str), begin(before), end(before) - 1);
    while (i != end(str)) {
        str.replace(i, i + RawSize - 1, after);
        i = search(begin(str), end(str), begin(before), end(before) - 1);
    }
}

struct log_event {
    log_event* next;
    string msg;
};

class logging_impl : public logging {

 public:

    void initialize() {
        m_thread = thread([this] { (*this)(); });
        log("TRACE", "logging", "run", __FILE__, __LINE__, invalid_actor_addr, "ENTRY");
    }

    void destroy() {
        log("TRACE", "logging", "run", __FILE__, __LINE__, invalid_actor_addr, "EXIT");
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

    void log(const char* level,
             const char* c_class_name,
             const char* function_name,
             const char* c_full_file_name,
             int line_num,
             actor_addr,
             const std::string& msg) {
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
        auto print_from = [&](ostream& oss) -> ostream& {
            /*TODO:
            if (!from) {
                if (strcmp(c_class_name, "logging") == 0) oss << "logging";
                else oss << "null";
            }
            else if (from->is_proxy()) oss << to_string(from);
            else {
#               ifdef CPPA_DEBUG_MODE
                oss << from.downcast<local_actor>()->debug_name();
#               else // CPPA_DEBUG_MODE
                oss << from->id() << "@local";
#               endif // CPPA_DEBUG_MODE
            }
            */
            return oss;
        };
        ostringstream line;
        line << time(0) << " "
             << level << " ";
                print_from(line) << " "
             << this_thread::get_id() << " "
             << class_name << " "
             << function_name << " "
             << file_name << ":" << line_num << " "
             << msg
             << endl;
        m_queue.push_back(new log_event{0, line.str()});
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
                                    actor_addr ptr,
                                    const std::string& msg)
: m_class(std::move(class_name)), m_fun_name(fun_name)
, m_file_name(file_name), m_line_num(line_num), m_self(std::move(ptr)) {
    get_logger()->log("TRACE", m_class.c_str(), fun_name,
                      file_name, line_num, m_self, "ENTRY " + msg);
}

logging::trace_helper::~trace_helper() {
    get_logger()->log("TRACE", m_class.c_str(), m_fun_name,
                      m_file_name, m_line_num, m_self, "EXIT");
}

logging::~logging() { }

logging* logging::create_singleton() { return new logging_impl; }

} // namespace cppa

namespace std {

const cppa::actor_ostream& endl(const cppa::actor_ostream& o) {
    return o.write("\n");
}

const cppa::actor_ostream& flush(const cppa::actor_ostream& o) {
    return o.flush();
}

} // namespace std
