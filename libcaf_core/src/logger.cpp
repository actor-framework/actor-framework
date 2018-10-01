/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <condition_variable>
#include <unordered_map>

#include "caf/config.hpp"

#include "caf/actor_proxy.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/get_process_id.hpp"
#include "caf/detail/pretty_type_name.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/intrusive/task_result.hpp"
#include "caf/local_actor.hpp"
#include "caf/locks.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/term.hpp"
#include "caf/timestamp.hpp"

namespace caf {

namespace {

constexpr const char* log_level_name[] = {
  "ERROR",
  "WARN",
  "INFO",
  "DEBUG",
  "TRACE"
};

#if CAF_LOG_LEVEL >= 0

#if defined(CAF_NO_THREAD_LOCAL)

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

#else // !CAF_NO_THREAD_LOCAL

thread_local intrusive_ptr<logger> current_logger;

inline void set_current_logger(logger* x) {
  current_logger.reset(x);
}

inline logger* get_current_logger() {
  return current_logger.get();
}

#endif // CAF_NO_THREAD_LOCAL

#else // CAF_LOG_LEVEL

inline void set_current_logger(logger*) {
  // nop
}

inline logger* get_current_logger() {
  return nullptr;
}
#endif // CAF_LOG_LEVEL

} // namespace <anonymous>

logger::event::event(int lvl, const char* cat, const char* fun, const char* fn,
                     int line, std::string msg, std::thread::id t, actor_id a,
                     timestamp ts)
    : level(lvl),
      category_name(cat),
      pretty_fun(fun),
      file_name(fn),
      line_number(line),
      message(std::move(msg)),
      tid(std::move(t)),
      aid(a),
      tstamp(ts) {
  // nop
}

logger::line_builder::line_builder() {
  // nop
}

logger::line_builder& logger::line_builder::
operator<<(const local_actor* self) {
  if (!str_.empty() && str_.back() != ' ')
    str_ += " ";
  str_ += self->name();
  return *this;
}

logger::line_builder& logger::line_builder::operator<<(const std::string& str) {
  if (!str_.empty() && str_.back() != ' ')
    str_ += " ";
  str_ += str;
  return *this;
}

logger::line_builder& logger::line_builder::operator<<(const char* str) {
  if (!str_.empty())
    str_ += " ";
  str_ += str;
  return *this;
}

logger::line_builder& logger::line_builder::operator<<(char x) {
  if (!str_.empty())
    str_ += " ";
  str_ += x;
  return *this;
}

std::string logger::line_builder::get() const {
  return std::move(str_);
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

void logger::log(event* x) {
  CAF_ASSERT(x->level >= 0 && x->level <= 4);
  if (has(inline_output_flag)) {
    std::unique_ptr<event> ptr{x};
    handle_event(*ptr);
  } else {
    queue_.synchronized_push_back(queue_mtx_, queue_cv_,x);
  }
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

bool logger::accepts(int level, const char* cname_begin,
                     const char* cname_end) {
  CAF_ASSERT(cname_begin != nullptr && cname_end != nullptr);
  CAF_ASSERT(level >= 0 && level <= 4);
  if (level > max_level_)
    return false;
  if (!component_filter.empty()) {
    auto it = std::search(component_filter.begin(), component_filter.end(),
                          cname_begin, cname_end);
    return it != component_filter.end();
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

logger::logger(actor_system& sys)
    : system_(sys),
      max_level_(CAF_LOG_LEVEL),
      flags_(0),
      queue_(policy{}) {
  // nop
}

namespace {
int to_level_int(const atom_value atom) {
  switch (atom_uint(atom)) {
    case atom_uint("quiet"):
    case atom_uint("QUIET"):
      return CAF_LOG_LEVEL_QUIET;
    case atom_uint("error"):
    case atom_uint("ERROR"):
      return CAF_LOG_LEVEL_ERROR;
    case atom_uint("warning"):
    case atom_uint("WARNING"):
      return CAF_LOG_LEVEL_WARNING;
    case atom_uint("info"):
    case atom_uint("INFO"):
      return CAF_LOG_LEVEL_INFO;
    case atom_uint("debug"):
    case atom_uint("DEBUG"):
      return CAF_LOG_LEVEL_DEBUG;
    case atom_uint("trace"):
    case atom_uint("TRACE"):
      return CAF_LOG_LEVEL_TRACE;
    default: {
      // nop
    }
  }
  return CAF_LOG_LEVEL_QUIET;
}
} // namespace

void logger::init(actor_system_config& cfg) {
  CAF_IGNORE_UNUSED(cfg);
#if CAF_LOG_LEVEL >= 0
  namespace lg = defaults::logger;
  component_filter = get_or(cfg, "logger.component-filter",
                            lg::component_filter);
  // Parse the configured log level.
  auto verbosity = get_if<atom_value>(&cfg, "logger.verbosity");
  auto file_verbosity = verbosity ? *verbosity : lg::file_verbosity;
  auto console_verbosity = verbosity ? *verbosity : lg::console_verbosity;
  file_verbosity = get_or(cfg, "logger.file-verbosity", file_verbosity);
  console_verbosity =
    get_or(cfg, "logger.console-verbosity", console_verbosity);
  file_level_ = to_level_int(file_verbosity);
  console_level_ = to_level_int(console_verbosity);
  max_level_ = std::max(file_level_, console_level_);
  // Parse the format string.
  file_format_ =
    parse_format(get_or(cfg, "logger.file-format", lg::file_format));
  console_format_ = parse_format(get_or(cfg,"logger.console-format",
                                        lg::console_format));
  // Set flags.
  if (get_or(cfg, "logger.inline-output", false))
    set(inline_output_flag);
  auto con_atm = get_or(cfg, "logger.console", lg::console);
  if (con_atm == atom("UNCOLORED"))
    set(uncolored_console_flag);
  else if (con_atm == atom("COLORED"))
    set(colored_console_flag);
#endif
}

void logger::render_fun_prefix(std::ostream& out, const char* pretty_fun) {
  auto first = pretty_fun;
  // set end to beginning of arguments
  const char* last = strchr(pretty_fun, '(');
  if (last == nullptr)
    return;
  auto strsize = static_cast<size_t>(last - first);
  auto jump_to_next_whitespace = [&] {
    // leave `first` unchanged if no whitespaces is present,
    // e.g., in constructor signatures
    auto tmp = std::find(first, last, ' ');
    if (tmp != last)
      first = tmp + 1;
  };
  // skip "virtual" prefix if present
  if (strncmp(pretty_fun, "virtual ", std::min<size_t>(strsize, 8)) == 0)
    jump_to_next_whitespace();
  // skip "static" prefix if present
  if (strncmp(pretty_fun, "static ", std::min<size_t>(strsize, 7)) == 0)
    jump_to_next_whitespace();
  // skip return type
  jump_to_next_whitespace();
  if (first == last)
    return;
  const char sep[] = "::"; // separator for namespaces and classes
  auto sep_first = std::begin(sep);
  auto sep_last = sep_first + 2; // using end() includes \0
  auto colons = first;
  decltype(colons) nextcs;
  while ((nextcs = std::search(colons + 1, last, sep_first, sep_last)) != last)
    colons = nextcs;
  std::string result;
  result.assign(first, colons);
  detail::prettify_type_name(result);
  out << result;
}

void logger::render_fun_name(std::ostream& out, const char* pretty_fun) {
  // Find the end of the function name by looking for the opening parenthesis
  // trailing it.
  CAF_ASSERT(pretty_fun != nullptr);
  const char* e = strchr(pretty_fun, '(');
  if (e == nullptr)
    return;
  /// Now look for the beginning of the function name.
  using rev_iter = std::reverse_iterator<const char*>;
  auto b = std::find_if(rev_iter(e), rev_iter(pretty_fun),
                        [](char x) { return x == ':' || x == ' '; });
  out.write(b.base(), e - b.base());
}

void logger::render_time_diff(std::ostream& out, timestamp t0, timestamp tn) {
  out << std::chrono::duration_cast<std::chrono::milliseconds>(tn - t0).count();
}

void logger::render_date(std::ostream& out, timestamp x) {
  out << deep_to_string(x);
}

void logger::render(std::ostream& out, const line_format& lf,
                    const event& x) const {
  for (auto& f : lf)
    switch (f.kind) {
      case category_field: out << x.category_name; break;
      case class_name_field: render_fun_prefix(out, x.pretty_fun); break;
      case date_field: render_date(out, x.tstamp); break;
      case file_field: out << x.file_name; break;
      case line_field: out << x.line_number; break;
      case message_field: out << x.message; break;
      case method_field: render_fun_name(out, x.pretty_fun); break;
      case newline_field: out << std::endl; break;
      case priority_field: out << log_level_name[x.level]; break;
      case runtime_field: render_time_diff(out, t0_, x.tstamp); break;
      case thread_field: out << x.tid; break;
      case actor_field: out << "actor" << x.aid; break;
      case percent_sign_field: out << '%'; break;
      case plain_text_field: out << f.text; break;
      default: ; // nop
    }
}

logger::line_format logger::parse_format(const std::string& format_str) {
  std::vector<field> res;
  auto plain_text_first = format_str.begin();
  bool read_percent_sign = false;
  auto i = format_str.begin();
  for (; i != format_str.end(); ++i) {
    if (read_percent_sign) {
      field_type ft;
      switch (*i) {
        case 'c': ft = category_field; break;
        case 'C': ft = class_name_field; break;
        case 'd': ft = date_field; break;
        case 'F': ft =  file_field; break;
        case 'L': ft = line_field; break;
        case 'm': ft = message_field; break;
        case 'M': ft = method_field; break;
        case 'n': ft = newline_field; break;
        case 'p': ft = priority_field; break;
        case 'r': ft = runtime_field; break;
        case 't': ft = thread_field; break;
        case 'a': ft = actor_field; break;
        case '%': ft = percent_sign_field; break;
        default:
          ft = invalid_field;
          std::cerr << "invalid field specifier in format string: "
                    << *i << std::endl;
      }
      if (ft != invalid_field)
        res.emplace_back(field{ft, std::string{}});
      plain_text_first = i + 1;
      read_percent_sign = false;
    } else {
      if (*i == '%') {
        if (plain_text_first != i)
          res.emplace_back(field{plain_text_field,
                                 std::string{plain_text_first, i}});
        read_percent_sign = true;
      }
    }
  }
  if (plain_text_first != i)
    res.emplace_back(field{plain_text_field, std::string{plain_text_first, i}});
  return res;
}

const char* logger::skip_path(const char* path) {
  auto ptr = strrchr(path, '/');
  return ptr == nullptr ? path : (ptr + 1);
}

void logger::run() {
#if CAF_LOG_LEVEL >= 0
  log_first_line();
  // receive log entries from other threads and actors
  bool stop = false;
  auto f = [&](event& x) {
    // empty message means: shut down
    if (x.message.empty()) {
      stop = true;
      return intrusive::task_result::stop;
    }
    handle_event(x);
    return intrusive::task_result::resume;
  };
  do {
    // make sure we have data to read and consume events
    queue_.synchronized_await(queue_mtx_, queue_cv_);
    queue_.new_round(1000, f);
  } while (!stop);
  log_last_line();
#endif
}

void logger::handle_file_event(event& x) {
  // Print to file if available.
  if (file_ && x.level <= file_level_)
    render(file_, file_format_, x);
}

void logger::handle_console_event(event& x) {
  // Stop if no console output is configured.
  if (!has(console_output_flag) || x.level > console_level_)
    return;
  if (has(uncolored_console_flag)) {
    render(std::clog, console_format_, x);
    std::clog << std::endl;
  } else {
    switch (x.level) {
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
    render(std::clog, console_format_, x);
    std::clog << term::reset_endl;
  }
}

void logger::handle_event(event& x) {
  handle_file_event(x);
  handle_console_event(x);
}

void logger::log_first_line() {
  auto make_event = [&](string_view config_val,
                        const atom_value default_val) -> event {
    std::string msg = "level = ";
    msg += to_string(get_or(system_.config(), config_val, default_val));
    msg += ", node = ";
    msg += to_string(system_.node());
    return {CAF_LOG_LEVEL_INFO,
            CAF_LOG_COMPONENT,
            CAF_PRETTY_FUN,
            __FILE__,
            __LINE__,
            std::move(msg),
            std::this_thread::get_id(),
            0,
            make_timestamp()};
  };
  event file = make_event("logger.file-verbosity", defaults::logger::file_verbosity);
  handle_file_event(file);
  event console = make_event("logger.console-verbosity", defaults::logger::console_verbosity);
  handle_console_event(console);
}

void logger::log_last_line() {
  event tmp{CAF_LOG_LEVEL_INFO,
            CAF_LOG_COMPONENT,
            CAF_PRETTY_FUN,
            __FILE__,
            __LINE__,
            "EOF",
            std::this_thread::get_id(),
            0,
            make_timestamp()};
  handle_event(tmp);
}

void logger::start() {
#if CAF_LOG_LEVEL >= 0
  parent_thread_ = std::this_thread::get_id();
  if (max_level_ == CAF_LOG_LEVEL_QUIET)
    return;
  t0_ = make_timestamp();
  auto f = get_or(system_.config(), "logger.file-name",
                  defaults::logger::file_name);
  if (f.empty()) {
    // No need to continue if console and log file are disabled.
    if (has(console_output_flag))
      return;
  } else {
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
      auto t0_str = timestamp_to_string(t0_);
      f.replace(i, i + sizeof(ts) - 1, t0_str);
    }
    const char node[] = "[NODE]";
    i = std::search(f.begin(), f.end(), std::begin(node), std::end(node) - 1);
    if (i != f.end()) {
      auto nid = to_string(system_.node());
      f.replace(i, i + sizeof(node) - 1, nid);
    }
    file_.open(f, std::ios::out | std::ios::app);
    if (!file_) {
      std::cerr << "unable to open log file " << f << std::endl;
      return;
    }
  }
  if (has(inline_output_flag))
    log_first_line();
  else
    thread_ = std::thread{[this] {
      detail::set_thread_name("caf.logger");
      this->system_.thread_started();
      this->run();
      this->system_.thread_terminates();
    }};
#endif
}

void logger::stop() {
#if CAF_LOG_LEVEL >= 0
  if (has(inline_output_flag)) {
    log_last_line();
    return;
  }
  if (!thread_.joinable())
    return;
  // an empty string means: shut down
  queue_.synchronized_push_back(queue_mtx_, queue_cv_, new event);
  thread_.join();
#endif
}

std::string to_string(logger::field_type x) {
  static constexpr const char* names[] = {
    "invalid", "category", "class_name", "date",         "file",
    "line",    "message",  "method",     "newline",      "priority",
    "runtime", "thread",   "actor",      "percent_sign", "plain_text"};
  return names[static_cast<size_t>(x)];
}

std::string to_string(const logger::field& x) {
  std::string result = "field{";
  result += to_string(x.kind);
  if (x.kind == logger::plain_text_field) {
    result += ", \"";
    result += x.text;
    result += '\"';
  }
  result += "}";
  return result;
}

bool operator==(const logger::field& x, const logger::field& y) {
  return x.kind == y.kind && x.text == y.text;
}

} // namespace caf
