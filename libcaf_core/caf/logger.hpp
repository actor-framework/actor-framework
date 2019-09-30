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

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

#include "caf/abstract_actor.hpp"
#include "caf/atom.hpp"
#include "caf/config.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/arg_wrapper.hpp"
#include "caf/detail/log_level.hpp"
#include "caf/detail/pretty_type_name.hpp"
#include "caf/detail/ringbuffer.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/shared_spinlock.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/singly_linked.hpp"
#include "caf/ref_counted.hpp"
#include "caf/string_view.hpp"
#include "caf/timestamp.hpp"
#include "caf/type_nr.hpp"
#include "caf/unifyn.hpp"

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
  // -- friends ----------------------------------------------------------------

  friend class actor_system;

  // -- constants --------------------------------------------------------------

  /// Configures the size of the circular event queue.
  static constexpr size_t queue_size = 128;

  // -- member types -----------------------------------------------------------

  /// Combines various logging-related flags and parameters into a bitfield.
  struct config {
    /// Stores `max(file_verbosity, console_verbosity)`.
    unsigned verbosity : 4;

    /// Configures the verbosity for file output.
    unsigned file_verbosity : 4;

    /// Configures the verbosity for console output.
    unsigned console_verbosity : 4;

    /// Configures whether the logger immediately writes its output in the
    /// calling thread, bypassing its queue. Use this option only in
    /// single-threaded test environments.
    bool inline_output : 1;

    /// Configures whether the logger generates colored output.
    bool console_coloring : 1;

    config();
  };

  /// Encapsulates a single logging event.
  struct event {
    // -- constructors, destructors, and assignment operators ------------------

    event() = default;

    event(event&&) = default;

    event(const event&) = default;

    event& operator=(event&&) = default;

    event& operator=(const event&) = default;

    event(unsigned lvl, unsigned line, atom_value cat, string_view full_fun,
          string_view fun, string_view fn, std::string msg, std::thread::id t,
          actor_id a, timestamp ts);

    // -- member variables -----------------------------------------------------

    /// Level/priority of the event.
    unsigned level;

    /// Current line in the file.
    unsigned line_number;

    /// Name of the category (component) logging the event.
    atom_value category_name;

    /// Name of the current function as reported by `__PRETTY_FUNCTION__`.
    string_view pretty_fun;

    /// Name of the current function as reported by `__func__`.
    string_view simple_fun;

    /// Name of the current file.
    string_view file_name;

    /// User-provided message.
    std::string message;

    /// Thread ID of the caller.
    std::thread::id tid;

    /// Actor ID of the caller.
    actor_id aid;

    /// Timestamp of the event.
    timestamp tstamp;
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

    line_builder& operator<<(string_view str);

    line_builder& operator<<(const char* str);

    line_builder& operator<<(char x);

    std::string get() const;

  private:
    std::string str_;
  };

  // -- constructors, destructors, and assignment operators --------------------

  ~logger() override;

  // -- logging ----------------------------------------------------------------

  /// Writes an entry to the event-queue of the logger.
  /// @thread-safe
  void log(event&& x);

  // -- properties -------------------------------------------------------------

  /// Returns the ID of the actor currently associated to the calling thread.
  actor_id thread_local_aid();

  /// Associates an actor ID to the calling thread and returns the last value.
  actor_id thread_local_aid(actor_id aid);

  /// Returns whether the logger is configured to accept input for given
  /// component and log level.
  bool accepts(unsigned level, atom_value component_name);

  /// Returns the output format used for the log file.
  const line_format& file_format() const {
    return file_format_;
  }

  /// Returns the output format used for the console.
  const line_format& console_format() const {
    return console_format_;
  }

  unsigned verbosity() const noexcept {
    return cfg_.verbosity;
  }

  unsigned file_verbosity() const noexcept {
    return cfg_.file_verbosity;
  }

  unsigned console_verbosity() const noexcept {
    return cfg_.console_verbosity;
  }

  // -- static utility functions -----------------------------------------------

  /// Renders the prefix (namespace and class) of a fully qualified function.
  static void render_fun_prefix(std::ostream& out, const event& x);

  /// Renders the name of a fully qualified function.
  static void render_fun_name(std::ostream& out, const event& x);

  /// Renders the difference between `t0` and `tn` in milliseconds.
  static void render_time_diff(std::ostream& out, timestamp t0, timestamp tn);

  /// Renders the date of `x` in ISO 8601 format.
  static void render_date(std::ostream& out, timestamp x);

  /// Parses `format_str` into a format description vector.
  /// @warning The returned vector can have pointers into `format_str`.
  static line_format parse_format(const std::string& format_str);

  /// Skips path in `filename`.
  static string_view skip_path(string_view filename);

  // -- utility functions ------------------------------------------------------

  /// Renders `x` using the line format `lf` to `out`.
  void render(std::ostream& out, const line_format& lf, const event& x) const;

  /// Returns a string representation of the joined groups of `x` if `x` is an
  /// actor with the `subscriber` mixin.
  template <class T>
  static
    typename std::enable_if<std::is_base_of<mixin::subscriber_base, T>::value,
                            std::string>::type
    joined_groups_of(const T& x) {
    return deep_to_string(x.joined_groups());
  }

  /// Returns a string representation of an empty list if `x` is not an actor
  /// with the `subscriber` mixin.
  template <class T>
  static
    typename std::enable_if<!std::is_base_of<mixin::subscriber_base, T>::value,
                            const char*>::type
    joined_groups_of(const T& x) {
    CAF_IGNORE_UNUSED(x);
    return "[]";
  }

  // -- thread-local properties ------------------------------------------------

  /// Stores the actor system for the current thread.
  static void set_current_actor_system(actor_system*);

  /// Returns the logger for the current thread or `nullptr` if none is
  /// registered.
  static logger* current_logger();

private:
  // -- constructors, destructors, and assignment operators --------------------

  logger(actor_system& sys);

  // -- initialization ---------------------------------------------------------

  void init(actor_system_config& cfg);

  bool open_file();

  // -- event handling ---------------------------------------------------------

  void handle_event(const event& x);

  void handle_file_event(const event& x);

  void handle_console_event(const event& x);

  void log_first_line();

  void log_last_line();

  // -- thread management ------------------------------------------------------

  void run();

  void start();

  void stop();

  // -- member variables -------------------------------------------------------

  // Configures verbosity and output generation.
  config cfg_;

  // Filters events by component name.
  std::vector<atom_value> component_blacklist;

  // References the parent system.
  actor_system& system_;

  // Identifies the thread that called `logger::start`.
  std::thread::id parent_thread_;

  // Timestamp of the first log event.
  timestamp t0_;

  // Format for generating file output.
  line_format file_format_;

  // Format for generating console output.
  line_format console_format_;

  // Stream for file output.
  std::fstream file_;

  // Filled with log events by other threads.
  detail::ringbuffer<event, queue_size> queue_;

  // Stores the assembled name of the log file.
  std::string file_name_;

  // Executes `logger::run`.
  std::thread thread_;
};

/// @relates logger::field_type
std::string to_string(logger::field_type x);

/// @relates logger::field
std::string to_string(const logger::field& x);

/// @relates logger::field
bool operator==(const logger::field& x, const logger::field& y);

} // namespace caf

// -- macro constants ----------------------------------------------------------

/// Expands to a no-op.
#define CAF_VOID_STMT static_cast<void>(0)

#ifndef CAF_LOG_COMPONENT
/// Name of the current component when logging.
#  define CAF_LOG_COMPONENT "caf"
#endif // CAF_LOG_COMPONENT

// -- utility macros -----------------------------------------------------------

#ifdef CAF_MSVC
/// Expands to a string representation of the current funciton name that
/// includes the full function name and its signature.
#  define CAF_PRETTY_FUN __FUNCSIG__
#else // CAF_MSVC
/// Expands to a string representation of the current funciton name that
/// includes the full function name and its signature.
#  define CAF_PRETTY_FUN __PRETTY_FUNCTION__
#endif // CAF_MSVC

/// Concatenates `a` and `b` to a single preprocessor token.
#define CAF_CAT(a, b) a##b

#define CAF_LOG_MAKE_EVENT(aid, component, loglvl, message)                    \
  ::caf::logger::event(loglvl, __LINE__, caf::atom(component), CAF_PRETTY_FUN, \
                       __func__, caf::logger::skip_path(__FILE__),             \
                       (::caf::logger::line_builder{} << message).get(),       \
                       ::std::this_thread::get_id(), aid,                      \
                       ::caf::make_timestamp())

/// Expands to `argument = <argument>` in log output.
#define CAF_ARG(argument) caf::detail::make_arg_wrapper(#argument, argument)

/// Expands to `argname = <argval>` in log output.
#define CAF_ARG2(argname, argval) caf::detail::make_arg_wrapper(argname, argval)

/// Expands to `argname = [argval, last)` in log output.
#define CAF_ARG3(argname, first, last)                                         \
  caf::detail::make_arg_wrapper(argname, first, last)

// -- logging macros -----------------------------------------------------------

#define CAF_LOG_IMPL(component, loglvl, message)                               \
  do {                                                                         \
    auto CAF_UNIFYN(caf_logger) = caf::logger::current_logger();               \
    if (CAF_UNIFYN(caf_logger) != nullptr                                      \
        && CAF_UNIFYN(caf_logger)->accepts(loglvl, caf::atom(component)))      \
      CAF_UNIFYN(caf_logger)                                                   \
        ->log(CAF_LOG_MAKE_EVENT(CAF_UNIFYN(caf_logger)->thread_local_aid(),   \
                                 component, loglvl, message));                 \
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

#  define CAF_LOG_TRACE(unused) CAF_VOID_STMT

#else // CAF_LOG_LEVEL < CAF_LOG_LEVEL_TRACE

#  define CAF_LOG_TRACE(entry_message)                                         \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_TRACE,                       \
                 "ENTRY" << entry_message);                                    \
    auto CAF_UNIFYN(caf_log_trace_guard_) = ::caf::detail::make_scope_guard(   \
      [=] { CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_TRACE, "EXIT"); })

#endif // CAF_LOG_LEVEL < CAF_LOG_LEVEL_TRACE

#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_DEBUG
#  define CAF_LOG_DEBUG(output)                                                \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_DEBUG, output)
#endif

#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_INFO
#  define CAF_LOG_INFO(output)                                                 \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_INFO, output)
#endif

#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_WARNING
#  define CAF_LOG_WARNING(output)                                              \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_WARNING, output)
#endif

#define CAF_LOG_ERROR(output)                                                  \
  CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_ERROR, output)

#ifndef CAF_LOG_INFO
#  define CAF_LOG_INFO(output) CAF_VOID_STMT
#  define CAF_LOG_INFO_IF(cond, output) CAF_VOID_STMT
#else // CAF_LOG_INFO
#  define CAF_LOG_INFO_IF(cond, output)                                        \
    if (cond)                                                                  \
      CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_INFO, output);             \
    CAF_VOID_STMT
#endif // CAF_LOG_INFO

#ifndef CAF_LOG_DEBUG
#  define CAF_LOG_DEBUG(output) CAF_VOID_STMT
#  define CAF_LOG_DEBUG_IF(cond, output) CAF_VOID_STMT
#else // CAF_LOG_DEBUG
#  define CAF_LOG_DEBUG_IF(cond, output)                                       \
    if (cond)                                                                  \
      CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_DEBUG, output);            \
    CAF_VOID_STMT
#endif // CAF_LOG_DEBUG

#ifndef CAF_LOG_WARNING
#  define CAF_LOG_WARNING(output) CAF_VOID_STMT
#  define CAF_LOG_WARNING_IF(cond, output) CAF_VOID_STMT
#else // CAF_LOG_WARNING
#  define CAF_LOG_WARNING_IF(cond, output)                                     \
    if (cond)                                                                  \
      CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_WARNING, output);          \
    CAF_VOID_STMT
#endif // CAF_LOG_WARNING

#ifndef CAF_LOG_ERROR
#  define CAF_LOG_ERROR(output) CAF_VOID_STMT
#  define CAF_LOG_ERROR_IF(cond, output) CAF_VOID_STMT
#else // CAF_LOG_ERROR
#  define CAF_LOG_ERROR_IF(cond, output)                                       \
    if (cond)                                                                  \
      CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_ERROR, output);            \
    CAF_VOID_STMT
#endif // CAF_LOG_ERROR

// -- macros for logging CE-0001 events ----------------------------------------

/// The log component responsible for logging control flow events that are
/// crucial for understanding happens-before relations. See RFC SE-0001.
#define CAF_LOG_FLOW_COMPONENT "caf_flow"

#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_DEBUG

#  define CAF_LOG_SPAWN_EVENT(ref, ctor_data)                                  \
    CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG,                  \
                 "SPAWN ; ID ="                                                \
                   << ref.id() << "; NAME =" << ref.name() << "; TYPE ="       \
                   << ::caf::detail::pretty_type_name(typeid(ref))             \
                   << "; ARGS =" << ctor_data.c_str()                          \
                   << "; NODE =" << ref.node()                                 \
                   << "; GROUPS =" << ::caf::logger::joined_groups_of(ref))

#  define CAF_LOG_SEND_EVENT(ptr)                                              \
    CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG,                  \
                 "SEND ; TO ="                                                 \
                   << ::caf::deep_to_string(                                   \
                        ::caf::strong_actor_ptr{this->ctrl()})                 \
                        .c_str()                                               \
                   << "; FROM =" << ::caf::deep_to_string(ptr->sender).c_str() \
                   << "; STAGES ="                                             \
                   << ::caf::deep_to_string(ptr->stages).c_str()               \
                   << "; CONTENT ="                                            \
                   << ::caf::deep_to_string(ptr->content()).c_str())

#  define CAF_LOG_RECEIVE_EVENT(ptr)                                           \
    CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG,                  \
                 "RECEIVE ; FROM ="                                            \
                   << ::caf::deep_to_string(ptr->sender).c_str()               \
                   << "; STAGES ="                                             \
                   << ::caf::deep_to_string(ptr->stages).c_str()               \
                   << "; CONTENT ="                                            \
                   << ::caf::deep_to_string(ptr->content()).c_str())

#  define CAF_LOG_REJECT_EVENT()                                               \
    CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG, "REJECT")

#  define CAF_LOG_ACCEPT_EVENT(unblocked)                                      \
    CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG,                  \
                 "ACCEPT ; UNBLOCKED =" << unblocked)

#  define CAF_LOG_DROP_EVENT()                                                 \
    CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG, "DROP")

#  define CAF_LOG_SKIP_EVENT()                                                 \
    CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG, "SKIP")

#  define CAF_LOG_FINALIZE_EVENT()                                             \
    CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG, "FINALIZE")

#  define CAF_LOG_SKIP_OR_FINALIZE_EVENT(invoke_result)                        \
    do {                                                                       \
      if (invoke_result == caf::invoke_message_result::skipped)                \
        CAF_LOG_SKIP_EVENT();                                                  \
      else                                                                     \
        CAF_LOG_FINALIZE_EVENT();                                              \
    } while (false)

#  define CAF_LOG_TERMINATE_EVENT(thisptr, rsn)                                \
    CAF_LOG_IMPL(CAF_LOG_FLOW_COMPONENT, CAF_LOG_LEVEL_DEBUG,                  \
                 "TERMINATE ; ID =" << thisptr->id() << "; REASON ="           \
                                    << deep_to_string(rsn).c_str()             \
                                    << "; NODE =" << thisptr->node())

#else // CAF_LOG_LEVEL >= CAF_LOG_LEVEL_DEBUG

#  define CAF_LOG_SPAWN_EVENT(ref, ctor_data) CAF_VOID_STMT

#  define CAF_LOG_SEND_EVENT(ptr) CAF_VOID_STMT

#  define CAF_LOG_RECEIVE_EVENT(ptr) CAF_VOID_STMT

#  define CAF_LOG_REJECT_EVENT() CAF_VOID_STMT

#  define CAF_LOG_ACCEPT_EVENT(unblocked) CAF_VOID_STMT

#  define CAF_LOG_DROP_EVENT() CAF_VOID_STMT

#  define CAF_LOG_SKIP_EVENT() CAF_VOID_STMT

#  define CAF_LOG_SKIP_OR_FINALIZE_EVENT(unused) CAF_VOID_STMT

#  define CAF_LOG_FINALIZE_EVENT() CAF_VOID_STMT

#  define CAF_LOG_TERMINATE_EVENT(thisptr, rsn) CAF_VOID_STMT

#endif // CAF_LOG_LEVEL >= CAF_LOG_LEVEL_DEBUG

// -- macros for logging streaming-related events ------------------------------

/// The log component for logging streaming-related events that are crucial for
/// understanding handshaking, credit decisions, etc.
#define CAF_LOG_STREAM_COMPONENT "caf_stream"

#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_DEBUG
#  define CAF_STREAM_LOG_DEBUG(output)                                         \
    CAF_LOG_IMPL(CAF_LOG_STREAM_COMPONENT, CAF_LOG_LEVEL_DEBUG, output)
#  define CAF_STREAM_LOG_DEBUG_IF(condition, output)                           \
    if (condition)                                                             \
    CAF_LOG_IMPL(CAF_LOG_STREAM_COMPONENT, CAF_LOG_LEVEL_DEBUG, output)
#else
#  define CAF_STREAM_LOG_DEBUG(unused) CAF_VOID_STMT
#  define CAF_STREAM_LOG_DEBUG_IF(unused1, unused2) CAF_VOID_STMT
#endif
