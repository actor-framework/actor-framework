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
#include <condition_variable>

#include "caf/config.hpp"

#if defined(CAF_LINUX) || defined(CAF_MACOS)
#include <unistd.h>
#include <cxxabi.h>
#include <sys/types.h>
#endif

#include "caf/string_algorithms.hpp"

#include "caf/all.hpp"
#include "caf/actor_proxy.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/get_process_id.hpp"
#include "caf/detail/single_reader_queue.hpp"

namespace caf {
namespace detail {

namespace {

struct pop_aid_log_event_t {
  constexpr pop_aid_log_event_t() {
    // nop
  }
};

constexpr auto pop_aid_log_event = pop_aid_log_event_t{};

struct log_event {
  log_event* next;
  log_event* prev;
  std::string msg;
  explicit log_event(std::string logmsg = "")
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
    const char* log_level_table[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
    thread_ = std::thread{[this] { (*this)(); }};
    std::string msg = "ENTRY log level = ";
    msg += log_level_table[global_log_level];
    log("TRACE", "logging", "run", __FILE__, __LINE__, msg);
  }

  void stop() override {
    log("TRACE", "logging", "run", __FILE__, __LINE__, "EXIT");
    // an empty string means: shut down
    queue_.synchronized_enqueue(queue_mtx_, queue_cv_, new log_event{""});
    thread_.join();
  }

  void operator()() {
    std::ostringstream fname;
    fname << "actor_log_" << get_process_id() << "_" << time(0) << ".log";
    std::fstream out(fname.str().c_str(), std::ios::out | std::ios::app);
    std::unique_ptr<log_event> event;
    for (;;) {
      // make sure we have data to read
      queue_.synchronized_await(queue_mtx_, queue_cv_);
      // read & process event
      event.reset(queue_.try_pop());
      CAF_ASSERT(event != nullptr);
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
         << "actor" << get_aid() << " " << std::this_thread::get_id() << " "
         << class_name << " " << function_name << " " << file_name << ":"
         << line_num << " " << msg << std::endl;
    queue_.synchronized_enqueue(queue_mtx_, queue_cv_,
                                 new log_event{line.str()});
  }

private:
  std::thread thread_;
  std::mutex queue_mtx_;
  std::condition_variable queue_cv_;
  detail::single_reader_queue<log_event> queue_;
};

} // namespace <anonymous>

logging::trace_helper::trace_helper(std::string class_name,
                                    const char* fun_name, const char* file_name,
                                    int line_num, const std::string& msg)
    : class_(std::move(class_name)),
      fun_name_(fun_name),
      file_name_(file_name),
      line_num_(line_num) {
  singletons::get_logger()->log("TRACE", class_.c_str(), fun_name, file_name,
                                line_num, "ENTRY " + msg);
}

logging::trace_helper::~trace_helper() {
  singletons::get_logger()->log("TRACE", class_.c_str(), fun_name_,
                                file_name_, line_num_, "EXIT");
}

logging::~logging() {
  // nop
}

logging* logging::create_singleton() {
  return new logging_impl;
}

// returns the actor ID for the current thread
actor_id logging::get_aid() {
  shared_lock<detail::shared_spinlock> guard{aids_lock_};
  auto i = aids_.find(std::this_thread::get_id());
  if (i != aids_.end()) {
    return i->second;
  }
  return 0;
}

actor_id logging::set_aid(actor_id aid) {
  auto tid = std::this_thread::get_id();
  upgrade_lock<detail::shared_spinlock> guard{aids_lock_};
  auto i = aids_.find(tid);
  if (i != aids_.end()) {
    // we modify it despite the shared lock because the elements themselves
    // are considered thread-local
    auto res = i->second;
    i->second = aid;
    return res;
  }
  // upgrade to unique lock and insert new element
  upgrade_to_unique_lock<detail::shared_spinlock> uguard{guard};
  aids_.emplace(tid, aid);
  return 0; // was empty before
}

} // namespace detail
} // namespace caf
