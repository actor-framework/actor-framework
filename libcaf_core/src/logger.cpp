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

#include "caf/logger.hpp"

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

#include "caf/locks.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/actor_system.hpp"

#include "caf/detail/get_process_id.hpp"
#include "caf/detail/single_reader_queue.hpp"

namespace caf {

namespace {

constexpr const char* log_level_name[] = {
  "ERROR",
  "WARN ",
  "INFO ",
  "DEBUG",
  "TRACE"
};

#ifdef CAF_LOG_LEVEL
static_assert(CAF_LOG_LEVEL >= 0 && CAF_LOG_LEVEL <= 4,
              "assertion: 0 <= CAF_LOG_LEVEL <= 4");

constexpr int global_log_level = CAF_LOG_LEVEL;

#ifdef CAF_MSVC
thread_local
#else
__thread
#endif
actor_system* current_logger_system_ptr = nullptr;

inline actor_system* current_logger_system() {
  return current_logger_system_ptr;
}

inline void current_logger_system(actor_system* x) {
  current_logger_system_ptr = x;
}

inline logger* get_current_logger() {
  auto sys = current_logger_system();
  return sys ? &sys->logger() : nullptr;
}
#else
inline void current_logger_system(actor_system*) {
  // nop
}

inline logger* get_current_logger() {
  return nullptr;
}
#endif

void prettify_type_name(std::string& class_name) {
  //replace_all(class_name, " ", "");
  replace_all(class_name, "::", ".");
  replace_all(class_name, "(anonymousnamespace)", "ANON");
  replace_all(class_name, ".__1.", "."); // gets rid of weird Clang-lib names
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
}

void prettify_type_name(std::string& class_name, const char* c_class_name) {
# if defined(CAF_LINUX) || defined(CAF_MACOS)
  int stat = 0;
  std::unique_ptr<char, decltype(free)*> real_class_name{nullptr, free};
  auto tmp = abi::__cxa_demangle(c_class_name, 0, 0, &stat);
  real_class_name.reset(tmp);
  class_name = stat == 0 ? real_class_name.get() : c_class_name;
# else
  class_name = c_class_name;
# endif
  prettify_type_name(class_name);
}

} // namespace <anonymous>

logger::event::event(std::string x)
    : next(nullptr),
      prev(nullptr),
      msg(std::move(x)) {
  // nop
}

logger::line_builder::line_builder() : behind_arg_(false) {
  // nop
}

logger::line_builder& logger::line_builder::operator<<(const std::string& str) {
  if (! str_.empty())
    str_ += " ";
  str_ += str;
  behind_arg_ = false;
  return *this;
}

logger::line_builder& logger::line_builder::operator<<(const char* str) {
  if (! str_.empty())
    str_ += " ";
  str_ += str;
  behind_arg_ = false;
  return *this;
}

std::string logger::line_builder::get() const {
  return std::move(str_);
}

std::string logger::render_type_name(const std::type_info& ti) {
  std::string result;
  prettify_type_name(result, ti.name());
  return result;
}

std::string logger::extract_class_name(const char* prettyfun, size_t strsize) {
  auto first = prettyfun;
  // set end to beginning of arguments
  auto last = std::find(first, prettyfun + strsize, '(');
  auto jump_to_next_whitespace = [&] {
    // leave `first` unchanged if no whitespaces is present,
    // e.g., in constructor signatures
    auto tmp = std::find(first, last, ' ');
    if (tmp != last)
      first = tmp + 1;
  };
  // skip "virtual" prefix if present
  if (strncmp(prettyfun, "virtual ", std::min<size_t>(strsize, 8)) == 0)
    jump_to_next_whitespace();
  // skip return type
  jump_to_next_whitespace();
  if (first == last)
    return "";
  const char sep[] = "::"; // separator for namespaces and classes
  auto sep_first = std::begin(sep);
  auto sep_last = sep_first + 2; // using end() includes \0
  auto colons = first;
  decltype(colons) nextcs;
  while ((nextcs = std::search(colons + 1, last, sep_first, sep_last)) != last)
    colons = nextcs;
  std::string result;
  result.assign(first, colons);
  prettify_type_name(result);
  return result;
}

// returns the actor ID for the current thread
actor_id logger::thread_local_aid() {
  shared_lock<detail::shared_spinlock> guard{aids_lock_};
  auto i = aids_.find(std::this_thread::get_id());
  if (i != aids_.end())
    return i->second;
  return 0;
}

actor_id logger::thread_local_aid(actor_id aid) {
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

void logger::log(int level, const char* component,
                 const std::string& class_name, const char* function_name,
                 const char* c_full_file_name, int line_num,
                 const std::string& msg) {
  CAF_ASSERT(level >= 0 && level <= 4);
  std::string file_name;
  std::string full_file_name = c_full_file_name;
  auto ri = find(full_file_name.rbegin(), full_file_name.rend(), '/');
  if (ri != full_file_name.rend()) {
    auto i = ri.base();
    if (i == full_file_name.end())
      file_name = std::move(full_file_name);
    else
      file_name = std::string(i, full_file_name.end());
  } else {
    file_name = std::move(full_file_name);
  }
  auto t0 = std::chrono::high_resolution_clock::now().time_since_epoch();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t0).count();
  std::ostringstream line;
  line << ms << " " << component << " " << log_level_name[level] << " "
       << "actor" << thread_local_aid() << " " << std::this_thread::get_id()
       << " " << class_name << " " << function_name << " " << file_name << ":"
       << line_num << " " << msg << std::endl;
  queue_.synchronized_enqueue(queue_mtx_, queue_cv_, new event{line.str()});
}

void logger::set_current_actor_system(actor_system* x) {
  current_logger_system(x);
}

logger* logger::current_logger() {
  return get_current_logger();
}

void logger::log_static(int level, const char* component,
                        const std::string& class_name,
                        const char* function_name, const char* file_name,
                        int line_num, const std::string& msg) {
  auto ptr = get_current_logger();
  if (ptr)
    ptr->log(level, component, class_name, function_name, file_name, line_num,
             msg);
}

logger::~logger() {
  // nop
}

logger::logger(actor_system& sys) : system_(sys) {
  // nop
}

void logger::run() {
  std::ostringstream fname;
  fname << "actor_log_" << detail::get_process_id() << "_" << time(0)
        << "_" << to_string(system_.node())
        << ".log";
  std::fstream out(fname.str().c_str(), std::ios::out | std::ios::app);
  std::unique_ptr<event> ptr;
  for (;;) {
    // make sure we have data to read
    queue_.synchronized_await(queue_mtx_, queue_cv_);
    // read & process event
    ptr.reset(queue_.try_pop());
    CAF_ASSERT(ptr != nullptr);
    if (ptr->msg.empty()) {
      out.close();
      return;
    }
    out << ptr->msg << std::flush;
  }
}

void logger::start() {
# ifdef CAF_LOG_LEVEL
  const char* log_level_table[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
  thread_ = std::thread{[this] { this->run(); }};
  std::string msg = "ENTRY log level = ";
  msg += log_level_table[global_log_level];
  log(4, "caf", "caf::logger", "run", __FILE__, __LINE__, msg);
# endif
}

void logger::stop() {
# ifdef CAF_LOG_LEVEL
  log(4, "caf", "caf::logger", "run", __FILE__, __LINE__, "EXIT");
  // an empty string means: shut down
  queue_.synchronized_enqueue(queue_mtx_, queue_cv_, new event{""});
  thread_.join();
# endif
}

} // namespace caf
