/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/term.hpp"
#include "caf/locks.hpp"
#include "caf/timestamp.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"

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

#ifdef CAF_MSVC

thread_local intrusive_ptr<logger> current_logger;

inline void set_current_logger(logger* x) {
  current_logger.reset(x);
}

inline logger* get_current_logger() {
  return current_logger.get();
}

#else // CAF_MSVC

pthread_key_t s_key;
pthread_once_t s_key_once = PTHREAD_ONCE_INIT;

void logger_ptr_destructor(void* ptr) {
  if (ptr != nullptr) {
    intrusive_ptr_release(reinterpret_cast<logger*>(ptr));
  }
}

void make_logger_ptr() {
  pthread_key_create(&s_key, logger_ptr_destructor);
}

void set_current_logger(logger* x) {
  pthread_once(&s_key_once, make_logger_ptr);
  logger_ptr_destructor(pthread_getspecific(s_key));
  if (x != nullptr)
    intrusive_ptr_add_ref(x);
  pthread_setspecific(s_key, x);
}

logger* get_current_logger() {
  pthread_once(&s_key_once, make_logger_ptr);
  return reinterpret_cast<logger*>(pthread_getspecific(s_key));
}

#endif // CAF_MSVC

#else // CAF_LOG_LEVEL

inline void set_current_logger(logger*) {
  // nop
}

inline logger* get_current_logger() {
  return nullptr;
}
#endif // CAF_LOG_LEVEL

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
  // finally, replace any whitespace with %20
  replace_all(class_name, " ", "%20");
}

void prettify_type_name(std::string& class_name, const char* c_class_name) {
# if defined(CAF_LINUX) || defined(CAF_MACOS)
  int stat = 0;
  std::unique_ptr<char, decltype(free)*> real_class_name{nullptr, free};
  auto tmp = abi::__cxa_demangle(c_class_name, nullptr, nullptr, &stat);
  real_class_name.reset(tmp);
  class_name = stat == 0 ? real_class_name.get() : c_class_name;
# else
  class_name = c_class_name;
# endif
  prettify_type_name(class_name);
}

} // namespace <anonymous>

logger::event::event(int lvl, std::string pfx, std::string content)
    : next(nullptr),
      prev(nullptr),
      level(lvl),
      prefix(std::move(pfx)),
      msg(std::move(content)) {
  // nop
}

logger::line_builder::line_builder() : behind_arg_(false) {
  // nop
}

logger::line_builder& logger::line_builder::operator<<(const std::string& str) {
  if (!str_.empty())
    str_ += " ";
  str_ += str;
  behind_arg_ = false;
  return *this;
}

logger::line_builder& logger::line_builder::operator<<(const char* str) {
  if (!str_.empty())
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

void logger::log_prefix(std::ostream& out, int level, const char* component,
                        const std::string& class_name,
                        const char* function_name, const char* c_full_file_name,
                        int line_num, const std::thread::id& tid) {
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
  std::ostringstream prefix;
  out << timestamp_to_string(make_timestamp()) << " " << component << " "
      << log_level_name[level] << " "
      << "actor" << thread_local_aid() << " " << tid << " "
      << class_name << " " << function_name << " "
      << file_name << ":" << line_num;
}

void logger::log(int level, const char* component,
                 const std::string& class_name, const char* function_name,
                 const char* c_full_file_name, int line_num,
                 const std::string& msg) {
  CAF_ASSERT(level >= 0 && level <= 4);
  std::ostringstream prefix;
  log_prefix(prefix, level, component, class_name, function_name,
             c_full_file_name, line_num);
  queue_.synchronized_enqueue(queue_mtx_, queue_cv_,
                              new event{level, prefix.str(), msg});
}

void logger::set_current_actor_system(actor_system* x) {
  if (x != nullptr)
    set_current_logger(&x->logger());
  else
    set_current_logger(nullptr);
}

logger* logger::current_logger() {
  return get_current_logger();
}

void logger::log_static(int level, const char* component,
                        const std::string& class_name,
                        const char* function_name, const char* file_name,
                        int line_num, const std::string& msg) {
  auto ptr = get_current_logger();
  if (ptr != nullptr)
    ptr->log(level, component, class_name, function_name, file_name, line_num,
             msg);
}

bool logger::accepts(int level, const char* component) {
  CAF_ASSERT(component != nullptr);
  CAF_ASSERT(level >= 0 && level <= 4);
  auto ptr = get_current_logger();
  if (ptr == nullptr || level > ptr->level_)
    return false;
  // TODO: once we've phased out GCC 4.8, we can upgrade this to a regex.
  // Until then, we just search for a substring in the filter.
  auto& filter = ptr->system_.config().logger_filter;
  if (!filter.empty()) {
    auto it = std::search(filter.begin(), filter.end(),
                          component, component + std::strlen(component));
    if (it == filter.end())
      return false;
  }
  return true;
}

logger::~logger() {
  stop();
  // tell system our dtor is done
  std::unique_lock<std::mutex> guard{system_.logger_dtor_mtx_};
  system_.logger_dtor_done_ = true;
  system_.logger_dtor_cv_.notify_one();
}

logger::logger(actor_system& sys) : system_(sys) {
  // nop
}

void logger::init(actor_system_config& cfg) {
  CAF_IGNORE_UNUSED(cfg);
#if defined(CAF_LOG_LEVEL)
  auto lvl_atom = cfg.logger_verbosity;
  switch (static_cast<uint64_t>(lvl_atom)) {
    case error_log_lvl_atom::uint_value():
      level_ = CAF_LOG_LEVEL_ERROR;
      break;
    case warning_log_lvl_atom::uint_value():
      level_ = CAF_LOG_LEVEL_WARNING;
      break;
    case info_log_lvl_atom::uint_value():
      level_ = CAF_LOG_LEVEL_INFO;
      break;
    case debug_log_lvl_atom::uint_value():
      level_ = CAF_LOG_LEVEL_DEBUG;
      break;
    case trace_log_lvl_atom::uint_value():
      level_ = CAF_LOG_LEVEL_TRACE;
      break;
    default: {
      level_ = CAF_LOG_LEVEL;
    }
  }
#endif
}

void logger::run() {
#if defined(CAF_LOG_LEVEL)
  std::fstream file;
  if (system_.config().logger_filename.empty()) {
    // If the console is also disabled, no need to continue.
    if (!(system_.config().logger_console == atom("UNCOLORED")
          || system_.config().logger_console == atom("COLORED")))
      return;
  } else {
    auto f = system_.config().logger_filename;
    // Replace placeholders.
    const char pid[] = "[PID]";
    auto i = std::search(f.begin(), f.end(), std::begin(pid),
                         std::end(pid) - 1);
    if (i != f.end()) {
      auto id = std::to_string(detail::get_process_id());
      f.replace(i, i + sizeof(pid) - 1, id);
    }
    const char ts[] = "[TIMESTAMP]";
    i = std::search(f.begin(), f.end(), std::begin(ts), std::end(ts) - 1);
    if (i != f.end()) {
      auto now = timestamp_to_string(make_timestamp());
      f.replace(i, i + sizeof(ts) - 1, now);
    }
    const char node[] = "[NODE]";
    i = std::search(f.begin(), f.end(), std::begin(node), std::end(node) - 1);
    if (i != f.end()) {
      auto nid = to_string(system_.node());
      f.replace(i, i + sizeof(node) - 1, nid);
    }
    file.open(f, std::ios::out | std::ios::app);
    if (!file) {
      std::cerr << "unable to open log file " << f << std::endl;
      return;
    }
  }
  if (file) {
    // log first entry
    constexpr atom_value level_names[] = {
      error_log_lvl_atom::value, warning_log_lvl_atom::value,
      info_log_lvl_atom::value, debug_log_lvl_atom::value,
      trace_log_lvl_atom::value};
    auto lvl_atom = level_names[CAF_LOG_LEVEL];
    log_prefix(file, CAF_LOG_LEVEL_INFO, "caf", "caf.logger", "start",
               __FILE__, __LINE__, parent_thread_);
    file << " level = " << to_string(lvl_atom) << ", node = "
         << to_string(system_.node()) << std::endl;
  }
  // receive log entries from other threads and actors
  std::unique_ptr<event> ptr;
  for (;;) {
    // make sure we have data to read
    queue_.synchronized_await(queue_mtx_, queue_cv_);
    // read & process event
    ptr.reset(queue_.try_pop());
    CAF_ASSERT(ptr != nullptr);
    // empty message means: shut down
    if (ptr->msg.empty())
      break;
    if (file)
      file << ptr->prefix << ' ' << ptr->msg << std::endl;
    if (system_.config().logger_console == atom("UNCOLORED")) {
      std::clog << ptr->msg << std::endl;
    } else if  (system_.config().logger_console == atom("COLORED")) {
      switch (ptr->level) {
        default:
          break;
        case CAF_LOG_LEVEL_ERROR:
          std::clog << term::red;
          break;
        case CAF_LOG_LEVEL_WARNING:
          std::clog << term::yellow;
          break;
        case CAF_LOG_LEVEL_INFO:
          std::clog << term::green;
          break;
        case CAF_LOG_LEVEL_DEBUG:
          std::clog << term::cyan;
          break;
        case CAF_LOG_LEVEL_TRACE:
          std::clog << term::blue;
          break;
      }
      std::clog << ptr->msg << term::reset << std::endl;
    }
  }
  if (file) {
    log_prefix(file, CAF_LOG_LEVEL_INFO, "caf", "caf.logger", "stop",
               __FILE__, __LINE__, parent_thread_);
    file << " EOF" << std::endl;
  }
#endif
}

void logger::start() {
#if defined(CAF_LOG_LEVEL)
  parent_thread_ = std::this_thread::get_id();
  if (system_.config().logger_verbosity == quiet_log_lvl_atom::value)
    return;
  thread_ = std::thread{[this] { this->run(); }};
#endif
}

void logger::stop() {
#if defined(CAF_LOG_LEVEL)
  if (!thread_.joinable())
    return;
  // an empty string means: shut down
  queue_.synchronized_enqueue(queue_mtx_, queue_cv_, new event);
  thread_.join();
#endif
}

} // namespace caf
