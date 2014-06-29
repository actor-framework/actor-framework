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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#include <ctime>
#include <thread>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <pthread.h>
#include <condition_variable>

#ifndef CPPA_WINDOWS
#include <unistd.h>
#include <sys/types.h>
#endif

#include "cppa/all.hpp"
#include "cppa/actor_proxy.hpp"

#include "cppa/detail/logging.hpp"
#include "cppa/detail/single_reader_queue.hpp"

namespace cppa {
namespace detail {

namespace {

__thread actor_id t_self_id;

template<size_t RawSize>
void replace_all(std::string& str, const char (&before)[RawSize],
                 const char* after) {
    // end(before) - 1 points to the null-terminator
    auto i = std::search(std::begin(str), std::end(str), std::begin(before),
                         std::end(before) - 1);
    while (i != std::end(str)) {
        str.replace(i, i + RawSize - 1, after);
        i = std::search(std::begin(str), std::end(str), std::begin(before),
                        std::end(before) - 1);
    }
}

constexpr struct pop_aid_log_event_t {
    constexpr pop_aid_log_event_t() {}
} pop_aid_log_event;

struct log_event {
    log_event* next;
    std::string msg;

};

class logging_impl : public logging {

 public:

    void initialize() {
        const char* log_level_lookup_table[] = {"ERROR", "WARN", "INFO",
                                                "DEBUG", "TRACE"};
        m_thread = std::thread([this] { (*this)(); });
        std::string msg = "ENTRY log level = ";
        msg += log_level_lookup_table[CPPA_LOG_LEVEL];
        log("TRACE", "logging", "run", __FILE__, __LINE__, msg);
    }

    void stop() {
        log("TRACE", "logging", "run", __FILE__, __LINE__, "EXIT");
        // an empty string means: shut down
        m_queue.synchronized_enqueue(m_queue_mtx, m_queue_cv,
                                     new log_event{0, ""});
        m_thread.join();
    }

    void operator()() {
        std::ostringstream fname;
        fname << "BoostActor_" << getpid() << "_" << time(0) << ".log";
        std::fstream out(fname.str().c_str(), std::ios::out | std::ios::app);
        std::unique_ptr<log_event> event;
        for (;;) {
            event.reset(m_queue.synchronized_pop(m_queue_mtx, m_queue_cv));
            if (event->msg.empty()) {
                out.close();
                return;
            } else
                out << event->msg << std::flush;
        }
    }

    void log(const char* level, const char* c_class_name,
             const char* function_name, const char* c_full_file_name,
             int line_num, const std::string& msg) override {
        std::string class_name = c_class_name;
        replace_all(class_name, "::", ".");
        replace_all(class_name, "(anonymous namespace)", "$anon$");
        std::string file_name;
        std::string full_file_name = c_full_file_name;
        auto ri = find(full_file_name.rbegin(), full_file_name.rend(), '/');
        if (ri != full_file_name.rend()) {
            auto i = ri.base();
            if (i == full_file_name.end())
                file_name = std::move(full_file_name);
            else
                file_name = std::string(i, full_file_name.end());
        } else
            file_name = std::move(full_file_name);
        std::ostringstream line;
        line << time(0) << " " << level << " "
             << "actor" << t_self_id << " " << std::this_thread::get_id() << " "
             << class_name << " " << function_name << " " << file_name << ":"
             << line_num << " " << msg << std::endl;
        m_queue.synchronized_enqueue(m_queue_mtx, m_queue_cv,
                                     new log_event{nullptr, line.str()});
    }

 private:

    std::thread m_thread;
    std::mutex m_queue_mtx;
    std::condition_variable m_queue_cv;
    detail::single_reader_queue<log_event> m_queue;

};

} // namespace <anonymous>

logging::trace_helper::trace_helper(std::string class_name,
                                    const char* fun_name, const char* file_name,
                                    int line_num, const std::string& msg)
        : m_class(std::move(class_name))
        , m_fun_name(fun_name)
        , m_file_name(file_name)
        , m_line_num(line_num) {
    singletons::get_logger()->log("TRACE", m_class.c_str(), fun_name, file_name,
                                  line_num, "ENTRY " + msg);
}

logging::trace_helper::~trace_helper() {
    singletons::get_logger()->log("TRACE", m_class.c_str(), m_fun_name,
                                  m_file_name, m_line_num, "EXIT");
}

logging::~logging() {}

logging* logging::create_singleton() { return new logging_impl; }

actor_id logging::set_aid(actor_id aid) {
    actor_id prev = t_self_id;
    t_self_id = aid;
    return prev;
}

} // namespace detail
} // namespace cppa
