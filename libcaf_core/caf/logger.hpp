/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_LOGGER_HPP
#define CAF_LOGGER_HPP

#include <thread>
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
#include "caf/ref_counted.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/deep_to_string.hpp"

#include "caf/detail/scope_guard.hpp"
#include "caf/detail/shared_spinlock.hpp"
#include "caf/detail/single_reader_queue.hpp"

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

  struct event {
    event* next;
    event* prev;
    int level;
    const char* component;
    std::string prefix;
    std::string msg;
    explicit event(int l = 0, char const* c = "", std::string p = "",
                   std::string m = "");
  };

  template <class T>
  struct arg_wrapper {
    const char* name;
    const T& value;
    arg_wrapper(const char* x, const T& y) : name(x), value(y) {
      // nop
    }
  };

  template <class T>
  static arg_wrapper<T> make_arg_wrapper(const char* name, const T& value) {
    return {name, value};
  }

  static std::string render_type_name(const std::type_info& ti);

  class line_builder {
  public:
    line_builder();

    template <class T>
    line_builder& operator<<(const T& x) {
      if (!str_.empty())
        str_ += " ";
      str_ += deep_to_string(x);
      behind_arg_ = false;
      return *this;
    }

    template <class T>
    line_builder& operator<<(const arg_wrapper<T>& x) {
      if (behind_arg_)
        str_ += ", ";
      else if (!str_.empty())
        str_ += " ";
      str_ += x.name;
      str_ += " = ";
      str_ += deep_to_string(x.value);
      behind_arg_ = true;
      return *this;
    }

    line_builder& operator<<(const std::string& str);

    line_builder& operator<<(const char* str);

    std::string get() const;

  private:
    std::string str_;
    bool behind_arg_;
  };

  static std::string extract_class_name(const char* prettyfun, size_t strsize);

  template <size_t N>
  static std::string extract_class_name(const char (&prettyfun)[N]) {
    return extract_class_name(prettyfun, N);
  }

  /// Returns the ID of the actor currently associated to the calling thread.
  actor_id thread_local_aid();

  /// Associates an actor ID to the calling thread and returns the last value.
  actor_id thread_local_aid(actor_id aid);

  /// Writes an entry to the log file.
  void log(int level, const char* component, const std::string& class_name,
           const char* function_name, const char* c_full_file_name,
           int line_num, const std::string& msg);

  ~logger() override;

  /** @cond PRIVATE */

  static void set_current_actor_system(actor_system*);

  static logger* current_logger();

  static void log_static(int level, const char* component,
                         const std::string& class_name,
                         const char* function_name,
                         const char* file_name, int line_num,
                         const std::string& msg);

  /** @endcond */

private:
  logger(actor_system& sys);

  void init(actor_system_config& cfg);

  void run();

  void start();

  void stop();

  void log_prefix(std::ostream& out, int level, const char* component,
                  const std::string& class_name, const char* function_name,
                  const char* c_full_file_name, int line_num,
                  const std::thread::id& tid = std::this_thread::get_id());

  actor_system& system_;
  int level_;
  detail::shared_spinlock aids_lock_;
  std::unordered_map<std::thread::id, actor_id> aids_;
  std::thread thread_;
  std::mutex queue_mtx_;
  std::condition_variable queue_cv_;
  detail::single_reader_queue<event> queue_;
  std::thread::id parent_thread_;
};

} // namespace caf

#define CAF_VOID_STMT static_cast<void>(0)

#define CAF_CAT(a, b) a##b

#define CAF_LOG_LEVEL_ERROR 0
#define CAF_LOG_LEVEL_WARNING 1
#define CAF_LOG_LEVEL_INFO 2
#define CAF_LOG_LEVEL_DEBUG 3
#define CAF_LOG_LEVEL_TRACE 4

#define CAF_ARG(argument) caf::logger::make_arg_wrapper(#argument, argument)

#ifdef CAF_MSVC
#define CAF_GET_CLASS_NAME caf::logger::extract_class_name(__FUNCSIG__)
#else // CAF_MSVC
#define CAF_GET_CLASS_NAME caf::logger::extract_class_name(__PRETTY_FUNCTION__)
#endif // CAF_MSVC

#ifndef CAF_LOG_LEVEL

#define CAF_LOG_IMPL(unused1, unused2, unused3)

// placeholder macros when compiling without logging
inline caf::actor_id caf_set_aid_dummy() { return 0; }

#define CAF_PUSH_AID(unused) CAF_VOID_STMT

#define CAF_PUSH_AID_FROM_PTR(unused) CAF_VOID_STMT

#define CAF_SET_AID(unused) caf_set_aid_dummy()

#define CAF_SET_LOGGER_SYS(unused) CAF_VOID_STMT

#define CAF_LOG_TRACE(unused)

#else // CAF_LOG_LEVEL

#define CAF_DEFAULT_LOG_COMPONENT "caf"

#ifndef CAF_LOG_COMPONENT
#define CAF_LOG_COMPONENT CAF_DEFAULT_LOG_COMPONENT
#endif

#define CAF_LOG_IMPL(component, loglvl, message)                               \
  caf::logger::log_static(loglvl, component, CAF_GET_CLASS_NAME,               \
                          __func__, __FILE__, __LINE__,                        \
                          (caf::logger::line_builder{} << message).get())

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
  const char* CAF_UNIFYN(func_name_) = __func__;                               \
  CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_TRACE,                         \
               "ENTRY" << entry_message);                                      \
  auto CAF_UNIFYN(caf_log_trace_guard_) = ::caf::detail::make_scope_guard([=] {\
    caf::logger::log_static(CAF_LOG_LEVEL_TRACE, CAF_LOG_COMPONENT,            \
                            CAF_GET_CLASS_NAME, CAF_UNIFYN(func_name_),        \
                            __FILE__, __LINE__, "EXIT");                       \
  })

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
  if (cond) { CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_DEBUG, output); }  \
    CAF_VOID_STMT

#define CAF_LOG_INFO_IF(cond, output)                                          \
  if (cond) { CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_INFO, output); }   \
    CAF_VOID_STMT

#define CAF_LOG_WARNING_IF(cond, output)                                       \
  if (cond) { CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_WARNING, output); }\
    CAF_VOID_STMT

#define CAF_LOG_ERROR_IF(cond, output)                                         \
  if (cond) { CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_ERROR, output); }  \
    CAF_VOID_STMT

#else // CAF_LOG_LEVEL

#define CAF_LOG_DEBUG_IF(unused1, unused2)
#define CAF_LOG_INFO_IF(unused1, unused2)
#define CAF_LOG_WARNING_IF(unused1, unused2)
#define CAF_LOG_ERROR_IF(unused1, unused2)

#endif // CAF_LOG_LEVEL

// -- Standardized CAF events according to SE-0001.

#define CAF_LOG_EVENT_COMPONENT "se-0001"

#define CAF_LOG_SPAWN_EVENT(aid, aargs)                                        \
  CAF_LOG_IMPL(CAF_LOG_EVENT_COMPONENT, CAF_LOG_LEVEL_DEBUG,                   \
               "SPAWN ; ID =" << aid                                           \
               << "; ARGS =" << deep_to_string(aargs).c_str())

#define CAF_LOG_INIT_EVENT(aName, aHide)                                       \
  CAF_LOG_IMPL(CAF_LOG_EVENT_COMPONENT, CAF_LOG_LEVEL_DEBUG,                   \
               "INIT ; NAME =" << aName << "; HIDDEN =" << aHide)

/// Logs
#define CAF_LOG_SEND_EVENT(ptr)                                                \
  CAF_LOG_IMPL(CAF_LOG_EVENT_COMPONENT, CAF_LOG_LEVEL_DEBUG,                   \
               "SEND ; TO ="                                                   \
               << deep_to_string(strong_actor_ptr{this->ctrl()}).c_str()       \
               << "; FROM =" << deep_to_string(ptr->sender).c_str()            \
               << "; STAGES =" << deep_to_string(ptr->stages).c_str()          \
               << "; CONTENT =" << deep_to_string(ptr->content()).c_str())

#define CAF_LOG_RECEIVE_EVENT(ptr)                                             \
  CAF_LOG_IMPL(CAF_LOG_EVENT_COMPONENT, CAF_LOG_LEVEL_DEBUG,                   \
               "RECEIVE ; FROM ="                                              \
               << deep_to_string(ptr->sender).c_str()                          \
               << "; STAGES =" << deep_to_string(ptr->stages).c_str()          \
               << "; CONTENT =" << deep_to_string(ptr->content()).c_str())

#define CAF_LOG_REJECT_EVENT()                                                 \
  CAF_LOG_IMPL(CAF_LOG_EVENT_COMPONENT, CAF_LOG_LEVEL_DEBUG, "REJECT")

#define CAF_LOG_ACCEPT_EVENT()                                                 \
  CAF_LOG_IMPL(CAF_LOG_EVENT_COMPONENT, CAF_LOG_LEVEL_DEBUG, "ACCEPT")

#define CAF_LOG_DROP_EVENT()                                                   \
  CAF_LOG_IMPL(CAF_LOG_EVENT_COMPONENT, CAF_LOG_LEVEL_DEBUG, "DROP")

#define CAF_LOG_SKIP_EVENT()                                                   \
  CAF_LOG_IMPL(CAF_LOG_EVENT_COMPONENT, CAF_LOG_LEVEL_DEBUG, "SKIP")

#define CAF_LOG_FINALIZE_EVENT()                                               \
  CAF_LOG_IMPL(CAF_LOG_EVENT_COMPONENT, CAF_LOG_LEVEL_DEBUG, "FINALIZE")

#define CAF_LOG_TERMINATE_EVENT(rsn)                                           \
  CAF_LOG_IMPL(CAF_LOG_EVENT_COMPONENT, CAF_LOG_LEVEL_DEBUG,                   \
               "TERMINATE ; REASON =" << deep_to_string(rsn).c_str())

#endif // CAF_LOGGER_HPP
