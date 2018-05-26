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

#include "caf/detail/pp.hpp"

namespace caf {
namespace detail {

struct any_char_t {};

constexpr any_char_t any_char = any_char_t{};

inline constexpr bool in_whitelist(any_char_t, char) {
  return true;
}

inline constexpr bool in_whitelist(char whitelist, char ch) {
  return whitelist == ch;
}

inline bool in_whitelist(const char* whitelist, char ch) {
  return strchr(whitelist, ch) != nullptr;
}

inline bool in_whitelist(bool (*filter)(char), char ch) {
  return filter(ch);
}

extern const char alphanumeric_chars[63];
extern const char alphabetic_chars[53];
extern const char hexadecimal_chars[23];
extern const char decimal_chars[11];
extern const char octal_chars[9];

} // namespace detail
} // namespace caf

#define CAF_FSM_EVAL_MISMATCH_EC                                               \
  if (mismatch_ec == ec::unexpected_character)                                 \
    ps.code = ch != '\n' ? mismatch_ec : ec::unexpected_newline;               \
  else                                                                         \
    ps.code = mismatch_ec;                                                     \
  return;

/// Starts the definition of an FSM.
#define start()                                                                \
  char ch = ps.current();                                                      \
  goto s_init;                                                                 \
  s_unexpected_eof:                                                            \
  __attribute__((unused));                                                     \
  ps.code = ec::unexpected_eof;                                                \
  return;                                                                      \
  {                                                                            \
    static constexpr ec mismatch_ec = ec::unexpected_character

/// Defines a non-terminal state in the FSM.
#define state(name)                                                            \
  CAF_FSM_EVAL_MISMATCH_EC                                                     \
  }                                                                            \
  {                                                                            \
    static constexpr ec mismatch_ec = ec::unexpected_character;                \
    s_##name : __attribute__((unused));                                        \
    if (ch == '\0')                                                            \
      goto s_unexpected_eof;                                                   \
    e_##name : __attribute__((unused));

/// Defines a terminal state in the FSM.
#define term_state(name)                                                       \
  CAF_FSM_EVAL_MISMATCH_EC                                                     \
  }                                                                            \
  {                                                                            \
    static constexpr ec mismatch_ec = ec::trailing_character;                  \
    s_##name : __attribute__((unused));                                        \
    if (ch == '\0')                                                            \
      goto s_fin;                                                              \
    e_##name : __attribute__((unused));

/// Ends the definition of an FSM.
#define fin()                                                                  \
  CAF_FSM_EVAL_MISMATCH_EC                                                     \
  }                                                                            \
  s_fin:                                                                       \
  ps.code = ec::success;                                                       \
  return;

#define CAF_TRANSITION_IMPL1(target)                                           \
  ch = ps.next();                                                              \
  goto s_##target;

#define CAF_TRANSITION_IMPL2(target, whitelist)                                \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    CAF_TRANSITION_IMPL1(target)                                               \
  }

#define CAF_TRANSITION_IMPL3(target, whitelist, action)                        \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    action;                                                                    \
    CAF_TRANSITION_IMPL1(target)                                               \
  }

#define CAF_TRANSITION_IMPL4(target, whitelist, action, error_code)            \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    if (!action) {                                                             \
      ps.code = error_code;                                                    \
      return;                                                                  \
    }                                                                          \
    CAF_TRANSITION_IMPL1(target)                                               \
  }

/// Transitions to target state if a predicate (optional argument 1) holds for
/// the current token and executes an action (optional argument 2) before
/// entering the new state.
#define transition(...)                                                        \
  CAF_PP_OVERLOAD(CAF_TRANSITION_IMPL, __VA_ARGS__)(__VA_ARGS__)

#define CAF_ERROR_TRANSITION_IMPL2(error_code, whitelist)                      \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    ps.code = error_code;                                                      \
    return;                                                                    \
  }

#define CAF_ERROR_TRANSITION_IMPL1(error_code)                                 \
  ps.code = error_code;                                                        \
  return;

/// Stops the FSM with reason `error_code` if `predicate` holds for the current
/// token.
#define error_transition(...)                                                  \
  CAF_PP_OVERLOAD(CAF_ERROR_TRANSITION_IMPL, __VA_ARGS__)(__VA_ARGS__)

#define CAF_EPSILON_IMPL1(target) goto s_##target;

#define CAF_EPSILON_IMPL2(target, whitelist)                                   \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    CAF_EPSILON_IMPL1(target)                                                  \
  }

#define CAF_EPSILON_IMPL3(target, whitelist, action)                           \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    action;                                                                    \
    CAF_EPSILON_IMPL1(target)                                                  \
  }

#define CAF_EPSILON_IMPL4(target, whitelist, action, error_code)               \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    if (!action) {                                                             \
      ps.code = error_code;                                                    \
      return;                                                                  \
    }                                                                          \
    CAF_EPSILON_IMPL1(target)                                                  \
  }

// Makes an epsilon transition into another state.
#define epsilon(...) CAF_PP_OVERLOAD(CAF_EPSILON_IMPL, __VA_ARGS__)(__VA_ARGS__)

// Makes an epsiolon transition into another state if the `statement` is true.
#define epsilon_if(statement, ...)                                             \
  if (statement) {                                                             \
    epsilon(__VA_ARGS__)                                                       \
  }

#define CAF_FSM_TRANSITION_IMPL2(fsm_call, target)                             \
  ps.next();                                                                   \
  fsm_call;                                                                    \
  if (ps.code > ec::trailing_character)                                        \
    return;                                                                    \
  ch = ps.current();                                                           \
  goto s_##target;

#define CAF_FSM_TRANSITION_IMPL3(fsm_call, target, whitelist)                  \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    CAF_FSM_TRANSITION_IMPL2(fsm_call, target)                                 \
  }

#define CAF_FSM_TRANSITION_IMPL4(fsm_call, target, whitelist, action)          \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    action;                                                                    \
    CAF_FSM_TRANSITION_IMPL2(fsm_call, target)                                 \
  }

#define CAF_FSM_TRANSITION_IMPL5(fsm_call, target, whitelist, action,          \
                                 error_code)                                   \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    if (!action) {                                                             \
      ps.code = error_code;                                                    \
      return;                                                                  \
    }                                                                          \
    CAF_FSM_TRANSITION_IMPL2(fsm_call, target)                                 \
  }

/// Makes an transition transition into another FSM, resuming at state `target`.
#define fsm_transition(...)                                                    \
  CAF_PP_OVERLOAD(CAF_FSM_TRANSITION_IMPL, __VA_ARGS__)(__VA_ARGS__)

#define fsm_transition_if(statement, ...)                                      \
  if (statement) {                                                             \
    fsm_transition(__VA_ARGS__)                                                \
  }

#define CAF_FSM_EPSILON_IMPL2(fsm_call, target)                                \
  fsm_call;                                                                    \
  if (ps.code > ec::trailing_character)                                        \
    return;                                                                    \
  ch = ps.current();                                                           \
  goto s_##target;

#define CAF_FSM_EPSILON_IMPL3(fsm_call, target, whitelist)                     \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    CAF_FSM_EPSILON_IMPL2(fsm_call, target)                                    \
  }

#define CAF_FSM_EPSILON_IMPL4(fsm_call, target, whitelist, action)             \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    action;                                                                    \
    CAF_FSM_EPSILON_IMPL2(fsm_call, target)                                    \
  }

#define CAF_FSM_EPSILON_IMPL5(fsm_call, target, whitelist, action, error_code) \
  if (::caf::detail::in_whitelist(whitelist, ch)) {                            \
    if (!action) {                                                             \
      ps.code = error_code;                                                    \
      return;                                                                  \
    }                                                                          \
    CAF_FSM_EPSILON_IMPL2(fsm_call, target)                                    \
  }

/// Makes an epsilon transition into another FSM, resuming at state `target`.
#define fsm_epsilon(...)                                                       \
  CAF_PP_OVERLOAD(CAF_FSM_EPSILON_IMPL, __VA_ARGS__)(__VA_ARGS__)

#define fsm_epsilon_if(statement, ...)                                         \
  if (statement) {                                                             \
    fsm_epsilon(__VA_ARGS__)                                                   \
  }

