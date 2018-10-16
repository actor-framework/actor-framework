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

#pragma once

#include <thread>
#include <fstream>
#include <cstring>
#include <sstream>
#include <iostream>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>

#include "caf/fwd.hpp"
#include "caf/config.hpp"
#include "caf/unifyn.hpp"
#include "caf/type_nr.hpp"
#include "caf/timestamp.hpp"
#include "caf/ref_counted.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/deep_to_string.hpp"

#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/singly_linked.hpp"

#include "caf/detail/arg_wrapper.hpp"
#include "caf/detail/pretty_type_name.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/shared_spinlock.hpp"

/*
 * To enable logging, you have to define CAF_DEBUG. This enables
 * CAF_LOG_ERROR messages. To enable more debugging output, you can
 * define CAF_LOG_LEVEL to:
 * 1: + warning
 * 2: + info
 * 3: + debug
 * 4: + trace (prints for each logged method entry and exit message)
 *
 * Note: this logger emits log4j style output; logs are best viewed
 *       using a log4j viewer, e.g., http://code.google.com/p/otroslogviewer/
 *
 */
namespace caf {

/// Centrally logs events from all actors in an actor system. To enable
/// logging in your application, you need to define `CAF_LOG_LEVEL`. Per
/// default, the logger generates log4j compatible output.
class logger : public ref_counted {
public:
  friend class actor_system;

  /// Encapsulates a single logging event.
  struct event : intrusive::singly_linked<event> {
    event() = default;

    event(int lvl, const char* cat, const char* fun, const char* fn, int line,
          std::string msg, std::thread::id t, actor_id a, timestamp ts);

    /// Level/priority of the event.
    int level;

    /// Name of the category (component) logging the event.
    const char* category_name;

    /// Name of the current context as reported by `__PRETTY_FUNCTION__`.
    const char* pretty_fun;

    /// Name of the current file.
    const char* file_name;

    /// Current line in the file.
    int line_number;

    /// User-provided message.
    std::string message;

    /// Thread ID of the caller.
    std::thread::id tid;

    /// Actor ID of the caller.
    actor_id aid;

    /// Timestamp of the event.
    timestamp tstamp;
  };

  struct policy {
    // -- member types ---------------------------------------------------------

    using mapped_type = event;

    using task_size_type = long;

    using deleter_type = std::default_delete<event>;

    using unique_pointer = std::unique_ptr<event, deleter_type>;

    using queue_type = intrusive::drr_queue<policy>;

    using deficit_type = long;

    // -- static member functions ----------------------------------------------

    static inline void release(mapped_type* ptr) noexcept {
      detail::disposer d;
      d(ptr);
    }

    static inline task_size_type task_size(const mapped_type&) noexcept {
      return 1;
    }

    static inline deficit_type quantum(const queue_type&, deficit_type x) {
      return x;
    }
  };

  /// Internal representation of format string entites.
  enum field_type {
    invalid_field,
    category_field,
    class_name_field,
    date_field,
    file_field,
    line_field,
    message_field,
    method_field,
    newline_field,
    priority_field,
    runtime_field,
    thread_field,
    actor_field,
    percent_sign_field,
    plain_text_field
  };

  /// Represents a single format string field.
  struct field {
    field_type kind;
    std::string text;
  };

  /// Stores a parsed format string as list of fields.
  using line_format = std::vector<field>;

  /// Utility class for building user-defined log messages with `CAF_ARG`.
  class line_builder {
  public:
    line_builder();

    template <class T>
    detail::enable_if_t<!std::is_pointer<T>::value, line_builder&>
    operator<<(const T& x) {
      if (!str_.empty())
        str_ += " ";
      str_ += deep_to_string(x);
      return *this;
    }

    line_builder& operator<<(const local_actor* self);

    line_builder& operator<<(const std::string& str);

    line_builder& operator<<(const char* str);

    line_builder& operator<<(char x);

    std::string get() const;

  private:
    std::string str_;
  };

  /// Returns the ID of the actor currently associated to the calling thread.
  actor_id thread_local_aid();

  /// Associates an actor ID to the calling thread and returns the last value.
  actor_id thread_local_aid(actor_id aid);

  /// Writes an entry to the log file.
  void log(event* x);

  ~logger() override;

  /** @cond PRIVATE */

  static void set_current_actor_system(actor_system*);

  static logger* current_logger();

  bool accepts(int level, const char* cname_begin, const char* cname_end);

  template <size_t N>
  bool accepts(int level, const char (&component)[N]) {
    return accepts(level, component, component + N - 1);
  }

  /** @endcond */

  /// Returns the output format used for the log file.
  inline const line_format& file_format() const {
    return file_format_;
  }

  /// Returns the output format used for the console.
  inline const line_format& console_format() const {
    return console_format_;
  }

  /// Renders the prefix (namespace and class) of a fully qualified function.
  static void render_fun_prefix(std::ostream& out, const char* pretty_fun);

  /// Renders the name of a fully qualified function.
  static void render_fun_name(std::ostream& out, const char* pretty_fun);

  /// Renders the difference between `t0` and `tn` in milliseconds.
  static void render_time_diff(std::ostream& out, timestamp t0, timestamp tn);

  /// Renders the date of `x` in ISO 8601 format.
  static void render_date(std::ostream& out, timestamp x);

  /// Renders `x` using the line format `lf` to `out`.
  void render(std::ostream& out, const line_format& lf, const event& x) const;

  /// Parses `format_str` into a format description vector.
  /// @warning The returned vector can have pointers into `format_str`.
  static line_format parse_format(const std::string& format_str);

  /// Skips path in `filename`.
  static const char* skip_path(const char* filename);

  /// Returns a string representation of the joined groups of `x` if `x` is an
  /// actor with the `subscriber` mixin.
  template <class T>
  static typename std::enable_if<
    std::is_base_of<mixin::subscriber_base, T>::value,
    std::string
  >::type
  joined_groups_of(const T& x) {
    return deep_to_string(x.joined_groups());
  }

  /// Returns a string representation of an empty list if `x` is not an actor
  /// with the `subscriber` mixin.
  template <class T>
  static typename std::enable_if<
    !std::is_base_of<mixin::subscriber_base, T>::value,
    const char*
  >::type
  joined_groups_of(const T& x) {
    CAF_IGNORE_UNUSED(x);
    return "[]";
  }

  // -- individual flags -------------------------------------------------------

  static constexpr int inline_output_flag = 0x01;

  static constexpr int uncolored_console_flag = 0x02;

  static constexpr int colored_console_flag = 0x04;

  // -- composed flags ---------------------------------------------------------

  static constexpr int console_output_flag = 0x06;

private:
  void handle_event(event& x);
  void handle_file_event(event& x);
  void handle_console_event(event& x);

  void log_first_line();

  void log_last_line();

  logger(actor_system& sys);

  void init(actor_system_config& cfg);

  void run();

  void start();

  void stop();

  inline bool has(int flag) const noexcept {
    return (flags_ & flag) != 0;
  }

  inline void set(int flag) noexcept {
    flags_ |= flag;
  }

  actor_system& system_;
  int max_level_;
  int console_level_;
  int file_level_;
  int flags_;
  detail::shared_spinlock aids_lock_;
  std::unordered_map<std::thread::id, actor_id> aids_;
  std::thread thread_;
  std::mutex queue_mtx_;
  std::condition_variable queue_cv_;
  intrusive::fifo_inbox<policy> queue_;
  std::thread::id parent_thread_;
  timestamp t0_;
  line_format file_format_;
  line_format console_format_;
  std::fstream file_;
  std::string component_filter;
};

std::string to_string(logger::field_type x);

std::string to_string(const logger::field& x);

bool operator==(const logger::field& x, const logger::field& y);

} // namespace caf

#define CAF_VOID_STMT static_cast<void>(0)

#define CAF_CAT(a, b) a##b

#define CAF_LOG_LEVEL_QUIET -1
#define CAF_LOG_LEVEL_ERROR 0
#define CAF_LOG_LEVEL_WARNING 1
#define CAF_LOG_LEVEL_INFO 2
#define CAF_LOG_LEVEL_DEBUG 3
#define CAF_LOG_LEVEL_TRACE 4

#define CAF_ARG(argument) caf::detail::make_arg_wrapper(#argument, argument)

#define CAF_ARG2(argname, argval) caf::detail::make_arg_wrapper(argname, argval)

#define CAF_ARG3(argname, first, last)                                         \
  caf::detail::make_arg_wrapper(argname, first, last)

#ifdef CAF_MSVC
#define CAF_PRETTY_FUN __FUNCSIG__
#else // CAF_MSVC
#define CAF_PRETTY_FUN __PRETTY_FUNCTION__
#endif // CAF_MSVC

#ifndef CAF_LOG_COMPONENT
#define CAF_LOG_COMPONENT "caf"
#endif // CAF_LOG_COMPONENT

#if CAF_LOG_LEVEL == -1

#define CAF_LOG_IMPL(unused1, unused2, unused3)

// placeholder macros when compiling without logging
inline caf::actor_id caf_set_aid_dummy() { return 0; }

#define CAF_PUSH_AID(unused) CAF_VOID_STMT

#define CAF_PUSH_AID_FROM_PTR(unused) CAF_VOID_STMT

#define CAF_SET_AID(unused) caf_set_aid_dummy()

#define CAF_SET_LOGGER_SYS(unused) CAF_VOID_STMT

#define CAF_LOG_TRACE(unused)

#else // CAF_LOG_LEVEL

#define CAF_LOG_IMPL(component, loglvl, message)                               \
  do {                                                                         \
    auto CAF_UNIFYN(caf_logger) = caf::logger::current_logger();               \
    if (CAF_UNIFYN(caf_logger) != nullptr                                      \
        && CAF_UNIFYN(caf_logger)->accepts(loglvl, component))                 \
      CAF_UNIFYN(caf_logger)                                                   \
        ->log(new ::caf::logger::event{                                        \
          loglvl, component, CAF_PRETTY_FUN, caf::logger::skip_path(__FILE__), \
          __LINE__, (::caf::logger::line_builder{} << message).get(),          \
          ::std::this_thread::get_id(),                                        \
          CAF_UNIFYN(caf_logger)->thread_local_aid(),                          \
          ::caf::make_timestamp()});                                           \
  } while (false)

#define CAF_PUSH_AID(aarg)                                                     \
  auto CAF_UNIFYN(caf_tmp_ptr) = caf::logger::current_logger();                \
  caf::actor_id CAF_UNIFYN(caf_aid_tmp) = 0;                                   \
  if (CAF_UNIFYN(caf_tmp_ptr))                                                 \
    CAF_UNIFYN(caf_aid_tmp) = CAF_UNIFYN(caf_tmp_ptr)->thread_local_aid(aarg); \
  auto CAF_UNIFYN(aid_aid_tmp_guard) = caf::detail::make_scope_guard([=] {     \
    auto CAF_UNIFYN(caf_tmp2_ptr) = caf::logger::current_logger();             \
    if (CAF_UNIFYN(caf_tmp2_ptr))                                              \
      CAF_UNIFYN(caf_tmp2_ptr)->thread_local_aid(CAF_UNIFYN(caf_aid_tmp));     \
  })

#define CAF_PUSH_AID_FROM_PTR(some_ptr)                                        \
  auto CAF_UNIFYN(caf_aid_ptr) = some_ptr;                                     \
  CAF_PUSH_AID(CAF_UNIFYN(caf_aid_ptr) ? CAF_UNIFYN(caf_aid_ptr)->id() : 0)

#define CAF_SET_AID(aid_arg)                                                   \
  (caf::logger::current_logger()                                               \
     ? caf::logger::current_logger()->thread_local_aid(aid_arg)                \
     : 0)

#define CAF_SET_LOGGER_SYS(ptr) caf::logger::set_current_actor_system(ptr)

#if CAF_LOG_LEVEL < CAF_LOG_LEVEL_TRACE

#define CAF_LOG_TRACE(unused) CAF_VOID_STMT

#else // CAF_LOG_LEVEL < CAF_LOG_LEVEL_TRACE

#define CAF_LOG_TRACE(entry_message)                                           \
  CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_TRACE,                         \
               "ENTRY" << entry_message);                                      \
  auto CAF_UNIFYN(caf_log_trace_guard_) = ::caf::detail::make_scope_guard(     \
    [=] { CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_TRACE, "EXIT"); })

#endif // CAF_LOG_LEVEL < CAF_LOG_LEVEL_TRACE

#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_DEBUG
#define CAF_LOG_DEBUG(output)                                                  \
  CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_DEBUG, output)
#endif

#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_INFO
#define CAF_LOG_INFO(output)                                                   \
  CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_INFO, output)
#endif

#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_WARNING
#define CAF_LOG_WARNING(output)                                                \
  CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_WARNING, output)
#endif

#define CAF_LOG_ERROR(output)                                                  \
  CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_ERROR, output)

#endif // CAF_LOG_LEVEL

#ifndef CAF_LOG_INFO
#  define CAF_LOG_INFO(output) CAF_VOID_STMT
#endif

#ifndef CAF_LOG_DEBUG
#  define CAF_LOG_DEBUG(output) CAF_VOID_STMT
#endif

#ifndef CAF_LOG_WARNING
#  define CAF_LOG_WARNING(output) CAF_VOID_STMT
#endif

#ifndef CAF_LOG_ERROR
#  define CAF_LOG_ERROR(output) CAF_VOID_STMT
#endif

#ifdef CAF_LOG_LEVEL

#define CAF_LOG_DEBUG_IF(cond, output)                                         \
  if (cond) {                                                                  \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_DEBUG, output);              \
  }                                                                            \
  CAF_VOID_STMT

#define CAF_LOG_INFO_IF(cond, output)                                          \
  if (cond) {                                                                  \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_INFO, output);               \
  }                                                                            \
  CAF_VOID_STMT

#define CAF_LOG_WARNING_IF(cond, output)                                       \
  if (cond) {                                                                  \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_WARNING, output);            \
  }                                                                            \
  CAF_VOID_STMT

#define CAF_LOG_ERROR_IF(cond, output)                                         \
  if (cond) {                                                                  \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_ERROR, output);              \
  }                                                                            \
  CAF_VOID_STMT

#else // CAF_LOG_LEVEL

#define CAF_LOG_DEBUG_IF(unused1, unused2)
#define CAF_LOG_INFO_IF(unused1, unused2)
#define CAF_LOG_WARNING_IF(unused1, unused2)
#define CAF_LOG_ERROR_IF(unused1, unused2)

#endif // CAF_LOG_LEVEL

// -- Standardized CAF events according to SE-0001.

/// The log component responsible for logging control flow events that are
/// crucial for understanding happens-before relations. See RFC SE-0001.
#define CAF_LOG_FLOW_COMPONENT "caf.flow"

#define CAF_LOG_SPAWN_EVENT(ref, ctor_data)                                    \
  CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG,                    \
               "SPAWN ; ID ="                                                  \
                 << ref.id() << "; NAME =" << ref.name()                       \
                 << "; TYPE =" << ::caf::detail::pretty_type_name(typeid(ref)) \
                 << "; ARGS =" << ctor_data.c_str()                            \
                 << "; NODE =" << ref.node()                                   \
                 << "; GROUPS =" << ::caf::logger::joined_groups_of(ref))

#define CAF_LOG_SEND_EVENT(ptr)                                                \
  CAF_LOG_IMPL(                                                                \
    CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG,                               \
    "SEND ; TO ="                                                              \
      << ::caf::deep_to_string(::caf::strong_actor_ptr{this->ctrl()}).c_str()  \
      << "; FROM =" << ::caf::deep_to_string(ptr->sender).c_str()              \
      << "; STAGES =" << ::caf::deep_to_string(ptr->stages).c_str()            \
      << "; CONTENT =" << ::caf::deep_to_string(ptr->content()).c_str())

#define CAF_LOG_RECEIVE_EVENT(ptr)                                             \
  CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG,                    \
               "RECEIVE ; FROM ="                                              \
                 << ::caf::deep_to_string(ptr->sender).c_str()                 \
                 << "; STAGES =" << ::caf::deep_to_string(ptr->stages).c_str() \
                 << "; CONTENT ="                                              \
                 << ::caf::deep_to_string(ptr->content()).c_str())

#define CAF_LOG_REJECT_EVENT()                                                 \
  CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG, "REJECT")

#define CAF_LOG_ACCEPT_EVENT(unblocked)                                        \
  CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG,                    \
               "ACCEPT ; UNBLOCKED =" << unblocked)

#define CAF_LOG_DROP_EVENT()                                                   \
  CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG, "DROP")

#define CAF_LOG_SKIP_EVENT()                                                   \
  CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG, "SKIP")

#define CAF_LOG_FINALIZE_EVENT()                                               \
  CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG, "FINALIZE")

#define CAF_LOG_TERMINATE_EVENT(thisptr, rsn)                                  \
  CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG,                    \
               "TERMINATE ; ID =" << thisptr->id()                             \
                 << "; REASON =" << deep_to_string(rsn).c_str()                \
                 << "; NODE =" << thisptr->node())
