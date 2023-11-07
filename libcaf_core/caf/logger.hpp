// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/config.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/arg_wrapper.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/format.hpp"
#include "caf/detail/log_level.hpp"
#include "caf/detail/pp.hpp"
#include "caf/detail/pretty_type_name.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/format_string_with_location.hpp"
#include "caf/fwd.hpp"

#include <cstring>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <typeinfo>

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
  // -- friends ----------------------------------------------------------------

  class trace_exit_guard;

  friend class actor_system;

  friend class trace_exit_guard;

  // -- member types -----------------------------------------------------------

  /// Stores context information for a log message.
  struct context {
    /// Severity of the message.
    unsigned level;

    /// Name of the component logging the event.
    std::string_view component;

    /// Current line in the file.
    unsigned line_number;

    /// Name of the current file.
    const char* file_name;

    /// Name of the current function.
    const char* function_name;
  };

  /// Helper class to print exit trace messages on scope exit.
  class trace_exit_guard {
  public:
    trace_exit_guard() = default;

    trace_exit_guard(logger* instance, context ctx)
      : instance_(instance), ctx_(ctx) {
      // nop
    }

    trace_exit_guard(trace_exit_guard&& other) {
      instance_ = other.instance_;
      if (instance_) {
        other.instance_ = nullptr;
        ctx_ = other.ctx_;
      }
    }

    trace_exit_guard& operator=(trace_exit_guard&& other) {
      using std::swap;
      swap(instance_, other.instance_);
      swap(ctx_, other.ctx_);
      return *this;
    }

    ~trace_exit_guard() {
      if (instance_)
        instance_->do_log(ctx_, std::string{"EXIT"});
    }

  private:
    logger* instance_ = nullptr;
    context ctx_;
  };

  /// Utility class for building user-defined log messages with `CAF_ARG`.
  class CAF_CORE_EXPORT line_builder {
  public:
    line_builder();

    template <class T>
    std::enable_if_t<!std::is_pointer_v<T>, line_builder&&>
    operator<<(const T& x) && {
      if (!str_.empty())
        str_ += " ";
      str_ += deep_to_string(x);
      return std::move(*this);
    }

    line_builder&& operator<<(const local_actor* self) &&;

    line_builder&& operator<<(const std::string& str) &&;

    line_builder&& operator<<(std::string_view str) &&;

    line_builder&& operator<<(const char* str) &&;

    line_builder&& operator<<(char x) &&;

    std::string get() && {
      return std::move(str_);
    }

  private:
    std::string str_;
  };

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~logger() = default;

  // -- logging ----------------------------------------------------------------

  /// Logs a message.
  /// @param level Severity of the message.
  /// @param component Name of the component logging the message.
  /// @param fmt_str The format string (with source location) for the message.
  /// @param args Arguments for the format string.
  template <class... Ts>
  static void log(unsigned level, std::string_view component,
                  format_string_with_location fmt_str, Ts&&... args) {
    auto* instance = current_logger();
    if (instance && instance->accepts(level, component)) {
      auto& loc = fmt_str.location;
      context ctx{level, component, loc.line(), loc.file_name(),
                  loc.function_name()};
      instance->do_log(ctx, detail::format(fmt_str.value,
                                           std::forward<Ts>(args)...));
    }
  }

  /// Logs a message with `trace` severity.
  /// @param component Name of the component logging the message.
  /// @param fmt_str The format string (with source location) for the message.
  /// @param args Arguments for the format string.
  template <class... Ts>
  [[nodiscard]] static trace_exit_guard
  trace(std::string_view component, format_string_with_location fmt_str,
        Ts&&... args) {
    auto* instance = current_logger();
    if (instance && instance->accepts(CAF_LOG_LEVEL_TRACE, component)) {
      std::string msg = "ENTRY ";
      detail::format_to(std::back_inserter(msg), fmt_str.value,
                        std::forward<Ts>(args)...);
      auto& loc = fmt_str.location;
      context ctx{CAF_LOG_LEVEL_TRACE, component, loc.line(), loc.file_name(),
                  loc.function_name()};
      instance->do_log(ctx, std::move(msg));
      return {instance, ctx};
    }
    return {nullptr, {}};
  }

  /// Logs a message with `debug` severity.
  /// @param component Name of the component logging the message.
  /// @param fmt_str The format string (with source location) for the message.
  /// @param args Arguments for the format string.
  template <class... Ts>
  static void debug(std::string_view component,
                    format_string_with_location fmt_str, Ts&&... args) {
    log(CAF_LOG_LEVEL_DEBUG, component, fmt_str, std::forward<Ts>(args)...);
  }

  /// Logs a message with `info` severity.
  /// @param component Name of the component logging the message.
  /// @param fmt_str The format string (with source location) for the message.
  /// @param args Arguments for the format string.
  template <class... Ts>
  static void info(std::string_view component,
                   format_string_with_location fmt_str, Ts&&... args) {
    log(CAF_LOG_LEVEL_INFO, component, fmt_str, std::forward<Ts>(args)...);
  }

  /// Logs a message with `warning` severity.
  /// @param component Name of the component logging the message.
  /// @param fmt_str The format string (with source location) for the message.
  /// @param args Arguments for the format string.
  template <class... Ts>
  static void warning(std::string_view component,
                      format_string_with_location fmt_str, Ts&&... args) {
    log(CAF_LOG_LEVEL_WARNING, component, fmt_str, std::forward<Ts>(args)...);
  }

  /// Logs a message with `error` severity.
  /// @param component Name of the component logging the message.
  /// @param fmt_str The format string (with source location) for the message.
  /// @param args Arguments for the format string.
  template <class... Ts>
  static void error(std::string_view component,
                    format_string_with_location fmt_str, Ts&&... args) {
    log(CAF_LOG_LEVEL_ERROR, component, fmt_str, std::forward<Ts>(args)...);
  }

  // -- legacy API (for the logging macros) ------------------------------------

  /// @private
  void legacy_api_log(unsigned level, std::string_view component,
                      std::string msg,
                      detail::source_location loc
                      = detail::source_location::current());

  /// @private
  [[nodiscard]] trace_exit_guard legacy_api_log_trace(
    unsigned level, std::string_view component, std::string msg,
    detail::source_location loc = detail::source_location::current()) {
    legacy_api_log(level, component, std::move(msg), loc);
    return {current_logger(), context{level, component, loc.line(),
                                      loc.file_name(), loc.function_name()}};
  }

  // -- properties -------------------------------------------------------------

  /// Returns the ID of the actor currently associated to the calling thread.
  static actor_id thread_local_aid();

  /// Associates an actor ID to the calling thread and returns the last value.
  static actor_id thread_local_aid(actor_id aid) noexcept;

  /// Returns whether the logger is configured to accept input for given
  /// component and log level.
  virtual bool accepts(unsigned level, std::string_view component_name) = 0;

  // -- static utility functions -----------------------------------------------

  /// Creates a new logger instance.
  static intrusive_ptr<logger> make(actor_system& sys);

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

private:
  // -- internal logging API ---------------------------------------------------

  virtual void do_log(const context& ctx, std::string&& msg) = 0;

  // -- initialization (called by the actor_system) ----------------------------

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
    if (auto* caf_logger_instance = caf::logger::current_logger();             \
        caf_logger_instance                                                    \
        && caf_logger_instance->accepts(loglvl, component)) {                  \
      caf_logger_instance->legacy_api_log(                                     \
        loglvl, component, (caf::logger::line_builder{} << message).get());    \
    }                                                                          \
  } while (false)

#define CAF_PUSH_AID(aarg)                                                     \
  caf::actor_id CAF_PP_UNIFYN(caf_aid_tmp)                                     \
    = caf::logger::thread_local_aid(aarg);                                     \
  auto CAF_PP_UNIFYN(caf_aid_tmp_guard)                                        \
    = caf::detail::scope_guard([=]() noexcept {                                \
        caf::logger::thread_local_aid(CAF_PP_UNIFYN(caf_aid_tmp));             \
      })

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
    caf::logger::trace_exit_guard caf_trace_log_auto_guard;                    \
    if (auto* caf_logger_instance = caf::logger::current_logger();             \
        caf_logger_instance                                                    \
        && caf_logger_instance->accepts(CAF_LOG_LEVEL_TRACE,                   \
                                        CAF_LOG_COMPONENT)) {                  \
      caf_logger_instance->legacy_api_log(                                     \
        CAF_LOG_LEVEL_TRACE, CAF_LOG_COMPONENT,                                \
        (caf::logger::line_builder{} << "ENTRY" << entry_message).get());      \
      auto caf_trace_loc = caf::detail::source_location::current();            \
      caf_trace_log_auto_guard = caf::logger::trace_exit_guard{                \
        caf_logger_instance,                                                   \
        caf::logger::context{CAF_LOG_LEVEL_TRACE, CAF_LOG_COMPONENT,           \
                             static_cast<unsigned>(caf_trace_loc.line()),      \
                             caf_trace_loc.file_name(),                        \
                             caf_trace_loc.function_name()}};                  \
    }                                                                          \
    static_cast<void>(0)

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
