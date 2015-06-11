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

#ifndef CAF_LOGGING_HPP
#define CAF_LOGGING_HPP

#include <thread>
#include <cstring>
#include <sstream>
#include <iostream>
#include <type_traits>
#include <unordered_map>

#include "caf/config.hpp"
#include "caf/to_string.hpp"
#include "caf/abstract_actor.hpp"

#include "caf/detail/singletons.hpp"
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
 * Note: this logger emits log4j style XML output; logs are best viewed
 *       using a log4j viewer, e.g., http://code.google.com/p/otroslogviewer/
 *
 */
namespace caf {
namespace detail {

class singletons;

class logging {
public:
  friend class detail::singletons;

  // returns the actor ID for the current thread or 0 if none is assigned
  actor_id get_aid();

  // associates given actor id with this thread,
  // returns the previously set actor id
  actor_id set_aid(actor_id aid);

  virtual void log(const char* level, const char* class_name,
                   const char* function_name, const char* file_name,
                   int line_num, const std::string& msg) = 0;

  class trace_helper {
  public:
    trace_helper(std::string class_name, const char* fun_name,
                 const char* file_name, int line_num, const std::string& msg);

    ~trace_helper();

  private:
    std::string class_;
    const char* fun_name_;
    const char* file_name_;
    int line_num_;
  };

protected:
  virtual ~logging();

  static logging* create_singleton();

  virtual void initialize() = 0;

  virtual void stop() = 0;

  inline void dispose() {
    delete this;
  }

private:
  detail::shared_spinlock aids_lock_;
  std::unordered_map<std::thread::id, actor_id> aids_;
};

struct oss_wr {
  inline oss_wr() {
    // nop
  }

  inline oss_wr(oss_wr&& other) : str_(std::move(other.str_)) {
    // nop
  }

  std::string str_;

  inline std::string str() {
    return std::move(str_);
  }
};

inline oss_wr operator<<(oss_wr&& lhs, std::string str) {
  lhs.str_ += std::move(str);
  return std::move(lhs);
}

inline oss_wr operator<<(oss_wr&& lhs, const char* str) {
  lhs.str_ += str;
  return std::move(lhs);
}

template <class T>
oss_wr operator<<(oss_wr&& lhs, T rhs) {
  std::ostringstream oss;
  oss << rhs;
  lhs.str_ += oss.str();
  return std::move(lhs);
}

} // namespace detail
} // namespace caf

#define CAF_VOID_STMT static_cast<void>(0)

#define CAF_CAT(a, b) a##b

#define CAF_ERROR 0
#define CAF_WARNING 1
#define CAF_INFO 2
#define CAF_DEBUG 3
#define CAF_TRACE 4

#define CAF_LVL_NAME0() "ERROR"
#define CAF_LVL_NAME1() "WARN "
#define CAF_LVL_NAME2() "INFO "
#define CAF_LVL_NAME3() "DEBUG"
#define CAF_LVL_NAME4() "TRACE"

#define CAF_CONCAT_(LHS, RHS) LHS ## RHS
#define CAF_CONCAT(LHS, RHS) CAF_CONCAT_(LHS, RHS)
#define CAF_UNIFYN(NAME) CAF_CONCAT(NAME, __LINE__)

#define CAF_PRINT_ERROR_IMPL(lvlname, classname, funname, message)             \
  {                                                                            \
    std::cerr << "[" << lvlname << "] " << classname << "::" << funname        \
              << ": " << message << "\n";                                      \
  }                                                                            \
  CAF_VOID_STMT

#ifndef CAF_LOG_LEVEL
inline caf::actor_id caf_set_aid_dummy() { return 0; }
#define CAF_LOG_IMPL(lvlname, classname, funname, message)                     \
  CAF_PRINT_ERROR_IMPL(lvlname, classname, funname, message)
#define CAF_PUSH_AID(unused) static_cast<void>(0)
#define CAF_PUSH_AID_FROM_PTR(unused) static_cast<void>(0)
#define CAF_SET_AID(unused) caf_set_aid_dummy()
#else
#define CAF_LOG_IMPL(lvlname, classname, funname, message)                     \
  if (strcmp(lvlname, "ERROR") == 0) {                                         \
    CAF_PRINT_ERROR_IMPL(lvlname, classname, funname, message);                \
  }                                                                            \
  caf::detail::singletons::get_logger()->log(lvlname, classname, funname,      \
                                             __FILE__, __LINE__,               \
                                             (caf::detail::oss_wr{}            \
                                              << std::boolalpha                \
                                              << message).str())
#define CAF_PUSH_AID(aid_arg)                                                  \
  auto CAF_UNIFYN(caf_aid_tmp)                                                 \
    = caf::detail::singletons::get_logger()->set_aid(aid_arg);                 \
  auto CAF_UNIFYN(aid_aid_tmp_guard) = caf::detail::make_scope_guard([=] {     \
    caf::detail::singletons::get_logger()->set_aid(CAF_UNIFYN(caf_aid_tmp));   \
  })
#define CAF_PUSH_AID_FROM_PTR(some_ptr)                                        \
  auto CAF_UNIFYN(caf_aid_ptr) = some_ptr;                                     \
  CAF_PUSH_AID(CAF_UNIFYN(caf_aid_ptr) ? CAF_UNIFYN(caf_aid_ptr)->id() : 0)
#define CAF_SET_AID(aid_arg)                                                   \
  caf::detail::singletons::get_logger()->set_aid(aid_arg)
#endif

#define CAF_PRINT0(lvlname, classname, funname, msg)                           \
  CAF_LOG_IMPL(lvlname, classname, funname, msg)

#define CAF_PRINT_IF0(stmt, lvlname, classname, funname, msg)                  \
  if (stmt) {                                                                  \
    CAF_LOG_IMPL(lvlname, classname, funname, msg);                            \
  }                                                                            \
  CAF_VOID_STMT

#define CAF_PRINT1(lvlname, classname, funname, msg)                           \
  CAF_PRINT0(lvlname, classname, funname, msg)

#define CAF_PRINT_IF1(stmt, lvlname, classname, funname, msg)                  \
  CAF_PRINT_IF0(stmt, lvlname, classname, funname, msg)

#if ! defined(CAF_LOG_LEVEL) || CAF_LOG_LEVEL < CAF_TRACE
#define CAF_PRINT4(arg0, arg1, arg2, arg3)
#else
#define CAF_PRINT4(lvlname, classname, funname, msg)                           \
  caf::detail::logging::trace_helper CAF_UNIFYN(caf_log_trace_)  {             \
    classname, funname, __FILE__, __LINE__,                                    \
      (caf::detail::oss_wr{} << msg).str()                                     \
  }
#endif

#if ! defined(CAF_LOG_LEVEL) || CAF_LOG_LEVEL < CAF_DEBUG
#define CAF_PRINT3(arg0, arg1, arg2, arg3)
#define CAF_PRINT_IF3(arg0, arg1, arg2, arg3, arg4)
#else
#define CAF_PRINT3(lvlname, classname, funname, msg)                           \
  CAF_PRINT0(lvlname, classname, funname, msg)
#define CAF_PRINT_IF3(stmt, lvlname, classname, funname, msg)                  \
  CAF_PRINT_IF0(stmt, lvlname, classname, funname, msg)
#endif

#if ! defined(CAF_LOG_LEVEL) || CAF_LOG_LEVEL < CAF_INFO
#define CAF_PRINT2(arg0, arg1, arg2, arg3)
#define CAF_PRINT_IF2(arg0, arg1, arg2, arg3, arg4)
#else
#define CAF_PRINT2(lvlname, classname, funname, msg)                           \
  CAF_PRINT0(lvlname, classname, funname, msg)
#define CAF_PRINT_IF2(stmt, lvlname, classname, funname, msg)                  \
  CAF_PRINT_IF0(stmt, lvlname, classname, funname, msg)
#endif

#define CAF_EVAL(what) what

/// @def CAF_LOGC
/// Logs a message with custom class and function names.
#define CAF_LOGC(level, classname, funname, msg)                               \
  CAF_CAT(CAF_PRINT, level)(CAF_CAT(CAF_LVL_NAME, level)(), classname,         \
                            funname, msg)

/// @def CAF_LOGF
/// Logs a message inside a free function.
#define CAF_LOGF(level, msg) CAF_LOGC(level, "NONE", __func__, msg)

/// @def CAF_LOGMF
/// Logs a message inside a member function.
#define CAF_LOGMF(level, msg)                                                  \
  CAF_LOGC(level, typeid(*this).name(), __func__, msg)

/// @def CAF_LOGC
/// Logs a message with custom class and function names.
#define CAF_LOGC_IF(stmt, level, classname, funname, msg)                      \
  CAF_CAT(CAF_PRINT_IF, level)(stmt, CAF_CAT(CAF_LVL_NAME, level)(),           \
                               classname, funname, msg)

/// @def CAF_LOGF
/// Logs a message inside a free function.
#define CAF_LOGF_IF(stmt, level, msg)                                          \
  CAF_LOGC_IF(stmt, level, "NONE", __func__, msg)

/// @def CAF_LOGMF
/// Logs a message inside a member function.
#define CAF_LOGMF_IF(stmt, level, msg)                                         \
  CAF_LOGC_IF(stmt, level, typeid(*this).name(), __func__, msg)

// convenience macros to safe some typing when printing arguments
#define CAF_ARG(arg) #arg << " = " << arg
#define CAF_TARG(arg, trans) #arg << " = " << trans(arg)
#define CAF_MARG(arg, memfun) #arg << " = " << arg.memfun()
#define CAF_TSARG(arg) #arg << " = " << to_string(arg)

/******************************************************************************
 *                             convenience macros                             *
 ******************************************************************************/

#define CAF_LOG_ERROR(msg) CAF_LOGMF(CAF_ERROR, msg)
#define CAF_LOG_WARNING(msg) CAF_LOGMF(CAF_WARNING, msg)
#define CAF_LOG_DEBUG(msg) CAF_LOGMF(CAF_DEBUG, msg)
#define CAF_LOG_INFO(msg) CAF_LOGMF(CAF_INFO, msg)
#define CAF_LOG_TRACE(msg) CAF_LOGMF(CAF_TRACE, msg)

#define CAF_LOG_ERROR_IF(stmt, msg) CAF_LOGMF_IF(stmt, CAF_ERROR, msg)
#define CAF_LOG_WARNING_IF(stmt, msg) CAF_LOGMF_IF(stmt, CAF_WARNING, msg)
#define CAF_LOG_DEBUG_IF(stmt, msg) CAF_LOGMF_IF(stmt, CAF_DEBUG, msg)
#define CAF_LOG_INFO_IF(stmt, msg) CAF_LOGMF_IF(stmt, CAF_INFO, msg)
#define CAF_LOG_TRACE_IF(stmt, msg) CAF_LOGMF_IF(stmt, CAF_TRACE, msg)

#define CAF_LOGF_ERROR(msg) CAF_LOGF(CAF_ERROR, msg)
#define CAF_LOGF_WARNING(msg) CAF_LOGF(CAF_WARNING, msg)
#define CAF_LOGF_DEBUG(msg) CAF_LOGF(CAF_DEBUG, msg)
#define CAF_LOGF_INFO(msg) CAF_LOGF(CAF_INFO, msg)
#define CAF_LOGF_TRACE(msg) CAF_LOGF(CAF_TRACE, msg)

#define CAF_LOGF_ERROR_IF(stmt, msg) CAF_LOGF_IF(stmt, CAF_ERROR, msg)
#define CAF_LOGF_WARNING_IF(stmt, msg) CAF_LOGF_IF(stmt, CAF_WARNING, msg)
#define CAF_LOGF_DEBUG_IF(stmt, msg) CAF_LOGF_IF(stmt, CAF_DEBUG, msg)
#define CAF_LOGF_INFO_IF(stmt, msg) CAF_LOGF_IF(stmt, CAF_INFO, msg)
#define CAF_LOGF_TRACE_IF(stmt, msg) CAF_LOGF_IF(stmt, CAF_TRACE, msg)

#endif // CAF_LOGGING_HPP
