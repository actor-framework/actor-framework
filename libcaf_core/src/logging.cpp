/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <ctime>
#include <thread>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <pthread.h>
#include <condition_variable>
#include <stdint.h>

#ifndef CAF_WINDOWS
#include <unistd.h>
#include <sys/types.h>
#endif

#include "caf/config.hpp"

#if defined(CAF_LINUX) || defined(CAF_MACOS)
# include <cxxabi.h>
#endif

#if defined(__APPLE__)
# include <TargetConditionals.h>
# if TARGET_OS_IPHONE == 1
#  define CAF_USE_PTHREAD_KEY
# endif
#elif defined(__ANDROID__)
# define CAF_USE_PTHREAD_KEY
#endif

#include "caf/string_algorithms.hpp"

#include "caf/all.hpp"
#include "caf/actor_proxy.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/single_reader_queue.hpp"

namespace caf {
namespace detail {

namespace {

#ifdef CAF_USE_PTHREAD_KEY

static pthread_key_t self_id_key;
static pthread_once_t self_id_key_once = PTHREAD_ONCE_INIT;

static void __make_self_id_key() {
  pthread_key_create(&self_id_key, NULL);
  pthread_setspecific(self_id_key, NULL);
}

static void self_id_init() {
  pthread_once(&self_id_key_once, __make_self_id_key);
}

static inline actor_id get_self_id() {
  return (actor_id)(intptr_t)pthread_getspecific(self_id_key);
}

static inline void set_self_id(actor_id aid) {
  pthread_setspecific(self_id_key, (const void *)(intptr_t)aid);
}

#else

static __thread actor_id t_self_id;

static void self_id_init() {
  // nop
}

static inline actor_id get_self_id() {
  return t_self_id;
}

static inline void set_self_id(actor_id aid) {
  t_self_id = aid;
}

#endif

constexpr struct pop_aid_log_event_t {
  constexpr pop_aid_log_event_t() {
    // nop
  }
} pop_aid_log_event;

struct log_event {
  log_event* next;
  log_event* prev;
  std::string msg;
  log_event(std::string logmsg = "")
      : next(nullptr),
        prev(nullptr),
        msg(std::move(logmsg)) {
    // nop
  }
};

#ifndef CAF_LOG_LEVEL
  constexpr int global_log_level = 0;
#else
  constexpr int global_log_level = CAF_LOG_LEVEL;
#endif

class logging_impl : public logging {
 public:
  void initialize() override {
    self_id_init();
    const char* log_level_table[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
    m_thread = std::thread([this] { (*this)(); });
    std::string msg = "ENTRY log level = ";
    msg += log_level_table[global_log_level];
    log("TRACE", "logging", "run", __FILE__, __LINE__, msg);
  }

  void stop() override {
    log("TRACE", "logging", "run", __FILE__, __LINE__, "EXIT");
    // an empty string means: shut down
    m_queue.synchronized_enqueue(m_queue_mtx, m_queue_cv, new log_event{""});
    m_thread.join();
  }

  void operator()() {
    std::ostringstream fname;
    fname << "actor_log_" << getpid() << "_" << time(0) << ".log";
    std::fstream out(fname.str().c_str(), std::ios::out | std::ios::app);
    std::unique_ptr<log_event> event;
    for (;;) {
      // make sure we have data to read
      m_queue.synchronized_await(m_queue_mtx, m_queue_cv);
      // read & process event
      event.reset(m_queue.try_pop());
      CAF_REQUIRE(event != nullptr);
      if (event->msg.empty()) {
        out.close();
        return;
      }
      out << event->msg << std::flush;
    }
  }

  void log(const char* level, const char* c_class_name,
           const char* function_name, const char* c_full_file_name,
           int line_num, const std::string& msg) override {
#   if defined(CAF_LINUX) || defined(CAF_MACOS)
    int stat = 0;
    std::unique_ptr<char, decltype(free)*> real_class_name{nullptr, free};
    auto tmp = abi::__cxa_demangle(c_class_name, 0, 0, &stat);
    real_class_name.reset(tmp);
    std::string class_name = stat == 0 ? real_class_name.get() : c_class_name;
    replace_all(class_name, " ", "");
    replace_all(class_name, "::", ".");
    replace_all(class_name, "(anonymousnamespace)", "$anon$");
    real_class_name.reset();
    // hide CAF magic in logs
    auto strip_magic = [&](const char* prefix_begin, const char* prefix_end) {
      auto last = class_name.end();
      auto i = std::search(class_name.begin(), last, prefix_begin, prefix_end);
      auto comma_or_angle_bracket = [](char c) { return c == ',' || c == '>'; };
      auto e = std::find_if(i, last, comma_or_angle_bracket);
      if (i != e) {
        std::string substr(i + (prefix_end - prefix_begin), e);
        class_name.swap(substr);
      }
    };
    char prefix1[] = "caf.detail.embedded<";
    strip_magic(prefix1, prefix1 + (sizeof(prefix1) - 1));
#   else
    std::string class_name = c_class_name;
#   endif
    std::string file_name;
    std::string full_file_name = c_full_file_name;
    auto ri = find(full_file_name.rbegin(), full_file_name.rend(), '/');
    if (ri != full_file_name.rend()) {
      auto i = ri.base();
      if (i == full_file_name.end()) {
        file_name = std::move(full_file_name);
      } else {
        file_name = std::string(i, full_file_name.end());
      }
    } else {
      file_name = std::move(full_file_name);
    }
    std::ostringstream line;
    line << time(0) << " " << level << " "
         << "actor" << get_self_id() << " " << std::this_thread::get_id() << " "
         << class_name << " " << function_name << " " << file_name << ":"
         << line_num << " " << msg << std::endl;
    m_queue.synchronized_enqueue(m_queue_mtx, m_queue_cv,
                                 new log_event{line.str()});
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
    : m_class(std::move(class_name)),
      m_fun_name(fun_name),
      m_file_name(file_name),
      m_line_num(line_num) {
  singletons::get_logger()->log("TRACE", m_class.c_str(), fun_name, file_name,
                                line_num, "ENTRY " + msg);
}

logging::trace_helper::~trace_helper() {
  singletons::get_logger()->log("TRACE", m_class.c_str(), m_fun_name,
                                m_file_name, m_line_num, "EXIT");
}

logging::~logging() {
  // nop
}

logging* logging::create_singleton() {
  return new logging_impl;
}

actor_id logging::set_aid(actor_id aid) {
  actor_id prev = get_self_id();
  set_self_id(aid);
  return prev;
}

} // namespace detail
} // namespace caf
