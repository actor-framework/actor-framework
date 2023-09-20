// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/config.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/arg_wrapper.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/log_level.hpp"
#include "caf/detail/pp.hpp"
#include "caf/detail/pretty_type_name.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/sync_ring_buffer.hpp"
#include "caf/fwd.hpp"
#include "caf/ref_counted.hpp"
#include "caf/timestamp.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <thread>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

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
class CAF_CORE_EXPORT logger {
public:
  // -- member types -----------------------------------------------------------

  /// Encapsulates a single logging event.
  struct CAF_CORE_EXPORT event {
    // -- constructors, destructors, and assignment operators ------------------

    event() = default;

    event(event&&) = default;

    event(const event&) = default;

    event& operator=(event&&) = default;

    event& operator=(const event&) = default;

    event(unsigned lvl, unsigned line, std::string_view cat,
          std::string_view full_fun, std::string_view fun, std::string_view fn,
          std::string msg, std::thread::id t, actor_id a, timestamp ts);

    // -- member variables -----------------------------------------------------

    /// Level/priority of the event.
    unsigned level;

    /// Current line in the file.
    unsigned line_number;

    /// Name of the category (component) logging the event.
    std::string_view category_name;

    /// Name of the current function as reported by `__PRETTY_FUNCTION__`.
    std::string_view pretty_fun;

    /// Name of the current function as reported by `__func__`.
    std::string_view simple_fun;

    /// Name of the current file.
    std::string_view file_name;

    /// User-provided message.
    std::string message;

    /// Thread ID of the caller.
    std::thread::id tid;

    /// Actor ID of the caller.
    actor_id aid;

    /// Timestamp of the event.
    timestamp tstamp;
  };

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

  /// Utility class for building user-defined log messages with `CAF_ARG`.
  class CAF_CORE_EXPORT line_builder {
  public:
    line_builder();

    template <class T>
    std::enable_if_t<!std::is_pointer_v<T>, line_builder&>
    operator<<(const T& x) {
      if (!str_.empty())
        str_ += " ";
      str_ += deep_to_string(x);
      return *this;
    }

    line_builder& operator<<(const local_actor* self);

    line_builder& operator<<(const std::string& str);

    line_builder& operator<<(std::string_view str);

    line_builder& operator<<(const char* str);

    line_builder& operator<<(char x);

    std::string get() const;

  private:
    std::string str_;
  };

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~logger() = default;

  // -- logging ----------------------------------------------------------------

  /// Writes an entry to the event-queue of the logger.
  /// @thread-safe
  virtual void log(event&& x) = 0;

  // -- properties -------------------------------------------------------------

  /// Returns the ID of the actor currently associated to the calling thread.
  static actor_id thread_local_aid();

  /// Associates an actor ID to the calling thread and returns the last value.
  static actor_id thread_local_aid(actor_id aid);

  /// Returns whether the logger is configured to accept input for given
  /// component and log level.
  virtual bool accepts(unsigned level, std::string_view component_name) = 0;

  // -- static utility functions -----------------------------------------------

  /// Creates a new logger instance.
  static intrusive_ptr<logger> make(actor_system& sys);

  /// Renders the prefix (namespace and class) of a fully qualified function.
  static void render_fun_prefix(std::ostream& out, const event& x);

  /// Skips path in `filename`.
  static std::string_view skip_path(std::string_view filename);

  // -- thread-local properties ------------------------------------------------

  /// Stores the actor system for the current thread.
  static void set_current_actor_system(actor_system*);

  /// Returns the logger for the current thread or `nullptr` if none is
  /// registered.
  static logger* current_logger();

  // -- reference counting -----------------------------------------------------

  /// Increases the reference count of the logger.
  virtual void ref_logger() const noexcept = 0;

  /// Decreases the reference count of the logger and destroys the object
  /// if necessary.
  virtual void deref_logger() const noexcept = 0;

  friend void intrusive_ptr_add_ref(const logger* ptr) noexcept {
    ptr->ref_logger();
  }

  friend void intrusive_ptr_release(const logger* ptr) noexcept {
    ptr->deref_logger();
  }

  // -- initialization ---------------------------------------------------------

  /// Allows the logger to read its configuration from the actor system config.
  virtual void init(const actor_system_config& cfg) = 0;

  /// Starts any background threads needed by the logger.
  virtual void start() = 0;

  /// Stops all background threads of the logger.
  virtual void stop() = 0;
};

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
/// Expands to a string representation of the current function name that
/// includes the full function name and its signature.
#  define CAF_PRETTY_FUN __FUNCSIG__
#else // CAF_MSVC
/// Expands to a string representation of the current function name that
/// includes the full function name and its signature.
#  define CAF_PRETTY_FUN __PRETTY_FUNCTION__
#endif // CAF_MSVC

/// Concatenates `a` and `b` to a single preprocessor token.
#define CAF_CAT(a, b) a##b

#define CAF_LOG_MAKE_EVENT(aid, component, loglvl, message)                    \
  ::caf::logger::event(loglvl, __LINE__, component, CAF_PRETTY_FUN, __func__,  \
                       caf::logger::skip_path(__FILE__),                       \
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
    auto CAF_PP_UNIFYN(caf_logger) = caf::logger::current_logger();            \
    if (CAF_PP_UNIFYN(caf_logger) != nullptr                                   \
        && CAF_PP_UNIFYN(caf_logger)->accepts(loglvl, component))              \
      CAF_PP_UNIFYN(caf_logger)                                                \
        ->log(CAF_LOG_MAKE_EVENT(caf::logger::thread_local_aid(), component,   \
                                 loglvl, message));                            \
  } while (false)

#define CAF_PUSH_AID(aarg)                                                     \
  caf::actor_id CAF_PP_UNIFYN(caf_aid_tmp)                                     \
    = caf::logger::thread_local_aid(aarg);                                     \
  auto CAF_PP_UNIFYN(caf_aid_tmp_guard) = caf::detail::make_scope_guard(       \
    [=] { caf::logger::thread_local_aid(CAF_PP_UNIFYN(caf_aid_tmp)); })

#define CAF_PUSH_AID_FROM_PTR(some_ptr)                                        \
  auto CAF_PP_UNIFYN(caf_aid_ptr) = some_ptr;                                  \
  CAF_PUSH_AID(CAF_PP_UNIFYN(caf_aid_ptr) ? CAF_PP_UNIFYN(caf_aid_ptr)->id()   \
                                          : 0)

#define CAF_SET_AID(aid_arg) caf::logger::thread_local_aid(aid_arg)

#define CAF_SET_LOGGER_SYS(ptr) caf::logger::set_current_actor_system(ptr)

#if CAF_LOG_LEVEL < CAF_LOG_LEVEL_TRACE

#  define CAF_LOG_TRACE(unused) CAF_VOID_STMT

#else // CAF_LOG_LEVEL < CAF_LOG_LEVEL_TRACE

#  define CAF_LOG_TRACE(entry_message)                                         \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_TRACE,                       \
                 "ENTRY" << entry_message);                                    \
    auto CAF_PP_UNIFYN(caf_log_trace_guard_)                                   \
      = ::caf::detail::make_scope_guard(                                       \
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

#if CAF_LOG_LEVEL >= CAF_LOG_LEVEL_ERROR
#  define CAF_LOG_ERROR(output)                                                \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_ERROR, output)
#endif

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
                   << "; NODE =" << ref.node())

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
