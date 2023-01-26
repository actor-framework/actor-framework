// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

// This header intentionally has no `#pragma once`. Any parser that uses this
// DSL is supposed to clean up all defines made in this header via
// `include "caf/detail/parser/fsm_undef.hpp"` at the end.

#include <type_traits>

#include "caf/detail/pp.hpp"

#define CAF_FSM_EVAL_ACTION(action)                                            \
  auto action_impl = [&]() -> decltype(auto) { return action; };               \
  if constexpr (std::is_same<pec, decltype(action_impl())>::value) {           \
    if (auto code = action_impl(); code != pec::success) {                     \
      ps.code = code;                                                          \
      return;                                                                  \
    }                                                                          \
  } else {                                                                     \
    action_impl();                                                             \
  }

#define CAF_FSM_EVAL_MISMATCH_EC                                               \
  if (mismatch_ec == caf::pec::unexpected_character)                           \
    ps.code = ch != '\n' ? mismatch_ec : caf::pec::unexpected_newline;         \
  else                                                                         \
    ps.code = mismatch_ec;                                                     \
  return;

/// Starts the definition of an FSM.
#define start()                                                                \
  char ch = ps.current();                                                      \
  goto s_init;                                                                 \
  s_unexpected_eof:                                                            \
  ps.code = caf::pec::unexpected_eof;                                          \
  return;                                                                      \
  {                                                                            \
    static_cast<void>(0); // dummy; init state closes parentheses

/// Defines a non-terminal state in the FSM.
#define state(name)                                                            \
  }                                                                            \
  for (;;) {                                                                   \
    /* jumps back up here if no transition matches */                          \
    ps.code = ch != '\n' ? caf::pec::unexpected_character                      \
                         : caf::pec::unexpected_newline;                       \
    return;                                                                    \
    s_##name : if (ch == '\0') goto s_unexpected_eof;                          \
    e_##name:

/// Defines a state in the FSM that doesn't check for end-of-input. Unstable
/// states must make a transition and cause undefined behavior otherwise.
#define unstable_state(name)                                                   \
  }                                                                            \
  {                                                                            \
    s_##name : e_##name:

/// Ends the definition of an FSM.
#define fin()                                                                  \
  }                                                                            \
  s_fin:                                                                       \
  ps.code = caf::pec::success;                                                 \
  return;

/// Defines a terminal state in the FSM.
#define CAF_TERM_STATE_IMPL1(name)                                             \
  }                                                                            \
  for (;;) {                                                                   \
    /* jumps back up here if no transition matches */                          \
    ps.code = caf::pec::trailing_character;                                    \
    return;                                                                    \
    s_##name : if (ch == '\0') goto s_fin;                                     \
    e_##name:

/// Defines a terminal state in the FSM that runs `exit_statement` when leaving
/// the state with code `pec::success` or `pec::trailing_character`.
#define CAF_TERM_STATE_IMPL2(name, exit_statement)                             \
  }                                                                            \
  for (;;) {                                                                   \
    /* jumps back up here if no transition matches */                          \
    ps.code = caf::pec::trailing_character;                                    \
    exit_statement;                                                            \
    return;                                                                    \
    s_##name : if (ch == '\0') {                                               \
      exit_statement;                                                          \
      goto s_fin;                                                              \
    }                                                                          \
    e_##name:

#define CAF_TRANSITION_IMPL1(target)                                           \
  ch = ps.next();                                                              \
  goto s_##target;

#define CAF_TRANSITION_IMPL2(target, whitelist)                                \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    CAF_TRANSITION_IMPL1(target)                                               \
  }

#define CAF_TRANSITION_IMPL3(target, whitelist, action)                        \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    CAF_FSM_EVAL_ACTION(action)                                                \
    CAF_TRANSITION_IMPL1(target)                                               \
  }

#define CAF_TRANSITION_IMPL4(target, whitelist, action, error_code)            \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    if (!action) {                                                             \
      ps.code = error_code;                                                    \
      return;                                                                  \
    }                                                                          \
    CAF_TRANSITION_IMPL1(target)                                               \
  }

#define CAF_ERROR_TRANSITION_IMPL2(error_code, whitelist)                      \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    ps.code = error_code;                                                      \
    return;                                                                    \
  }

#define CAF_ERROR_TRANSITION_IMPL1(error_code)                                 \
  ps.code = error_code;                                                        \
  return;

#define CAF_EPSILON_IMPL1(target) goto s_##target;

#define CAF_EPSILON_IMPL2(target, whitelist)                                   \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    CAF_EPSILON_IMPL1(target)                                                  \
  }

#define CAF_EPSILON_IMPL3(target, whitelist, action)                           \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    CAF_FSM_EVAL_ACTION(action)                                                \
    CAF_EPSILON_IMPL1(target)                                                  \
  }

#define CAF_EPSILON_IMPL4(target, whitelist, action, error_code)               \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    if (!action) {                                                             \
      ps.code = error_code;                                                    \
      return;                                                                  \
    }                                                                          \
    CAF_EPSILON_IMPL1(target)                                                  \
  }

#define CAF_FSM_TRANSITION_IMPL2(fsm_call, target)                             \
  ps.next();                                                                   \
  fsm_call;                                                                    \
  if (ps.code > caf::pec::trailing_character)                                  \
    return;                                                                    \
  ch = ps.current();                                                           \
  goto s_##target;

#define CAF_FSM_TRANSITION_IMPL3(fsm_call, target, whitelist)                  \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    CAF_FSM_TRANSITION_IMPL2(fsm_call, target)                                 \
  }

#define CAF_FSM_TRANSITION_IMPL4(fsm_call, target, whitelist, action)          \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    CAF_FSM_EVAL_ACTION(action)                                                \
    CAF_FSM_TRANSITION_IMPL2(fsm_call, target)                                 \
  }

#define CAF_FSM_TRANSITION_IMPL5(fsm_call, target, whitelist, action,          \
                                 error_code)                                   \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    if (!action) {                                                             \
      ps.code = error_code;                                                    \
      return;                                                                  \
    }                                                                          \
    CAF_FSM_TRANSITION_IMPL2(fsm_call, target)                                 \
  }

#define CAF_FSM_EPSILON_IMPL2(fsm_call, target)                                \
  fsm_call;                                                                    \
  if (ps.code > caf::pec::trailing_character)                                  \
    return;                                                                    \
  ch = ps.current();                                                           \
  goto s_##target;

#define CAF_FSM_EPSILON_IMPL3(fsm_call, target, whitelist)                     \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    CAF_FSM_EPSILON_IMPL2(fsm_call, target)                                    \
  }

#define CAF_FSM_EPSILON_IMPL4(fsm_call, target, whitelist, action)             \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    CAF_FSM_EVAL_ACTION(action)                                                \
    CAF_FSM_EPSILON_IMPL2(fsm_call, target)                                    \
  }

#define CAF_FSM_EPSILON_IMPL5(fsm_call, target, whitelist, action, error_code) \
  if (::caf::detail::parser::in_whitelist(whitelist, ch)) {                    \
    if (!action) {                                                             \
      ps.code = error_code;                                                    \
      return;                                                                  \
    }                                                                          \
    CAF_FSM_EPSILON_IMPL2(fsm_call, target)                                    \
  }

#ifdef CAF_MSVC

/// Defines a terminal state in the FSM.
#  define term_state(...)                                                      \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_TERM_STATE_IMPL, __VA_ARGS__)(__VA_ARGS__), \
               CAF_PP_EMPTY())

/// Transitions to target state if a predicate (optional argument 1) holds for
/// the current token and executes an action (optional argument 2) before
/// entering the new state.
#  define transition(...)                                                      \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_TRANSITION_IMPL, __VA_ARGS__)(__VA_ARGS__), \
               CAF_PP_EMPTY())

/// Stops the FSM with reason `error_code` if `predicate` holds for the current
/// token.
#  define error_transition(...)                                                \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_ERROR_TRANSITION_IMPL,                      \
                               __VA_ARGS__)(__VA_ARGS__),                      \
               CAF_PP_EMPTY())

// Makes an epsilon transition into another state.
#  define epsilon(...)                                                         \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_EPSILON_IMPL, __VA_ARGS__)(__VA_ARGS__),    \
               CAF_PP_EMPTY())

/// Makes an transition transition into another FSM, resuming at state `target`.
#  define fsm_transition(...)                                                  \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_FSM_TRANSITION_IMPL,                        \
                               __VA_ARGS__)(__VA_ARGS__),                      \
               CAF_PP_EMPTY())

/// Makes an epsilon transition into another FSM, resuming at state `target`.
#  define fsm_epsilon(...)                                                     \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_FSM_EPSILON_IMPL,                           \
                               __VA_ARGS__)(__VA_ARGS__),                      \
               CAF_PP_EMPTY())

#else // CAF_MSVC

/// Defines a terminal state in the FSM.
#  define term_state(...)                                                      \
    CAF_PP_OVERLOAD(CAF_TERM_STATE_IMPL, __VA_ARGS__)(__VA_ARGS__)

/// Transitions to target state if a predicate (optional argument 1) holds for
/// the current token and executes an action (optional argument 2) before
/// entering the new state.
#  define transition(...)                                                      \
    CAF_PP_OVERLOAD(CAF_TRANSITION_IMPL, __VA_ARGS__)(__VA_ARGS__)

/// Stops the FSM with reason `error_code` if `predicate` holds for the current
/// token.
#  define error_transition(...)                                                \
    CAF_PP_OVERLOAD(CAF_ERROR_TRANSITION_IMPL, __VA_ARGS__)(__VA_ARGS__)

// Makes an epsilon transition into another state.
#  define epsilon(...)                                                         \
    CAF_PP_OVERLOAD(CAF_EPSILON_IMPL, __VA_ARGS__)(__VA_ARGS__)

/// Makes an transition transition into another FSM, resuming at state `target`.
#  define fsm_transition(...)                                                  \
    CAF_PP_OVERLOAD(CAF_FSM_TRANSITION_IMPL, __VA_ARGS__)(__VA_ARGS__)

/// Makes an epsilon transition into another FSM, resuming at state `target`.
#  define fsm_epsilon(...)                                                     \
    CAF_PP_OVERLOAD(CAF_FSM_EPSILON_IMPL, __VA_ARGS__)(__VA_ARGS__)

#endif // CAF_MSVC

/// Enables a transition into another state if the `statement` is true.
#define transition_if(statement, ...)                                          \
  if (statement) {                                                             \
    transition(__VA_ARGS__)                                                    \
  }

/// Enables a transition if the constexpr `statement` is true.
#define transition_static_if(statement, ...)                                   \
  if constexpr (statement) {                                                   \
    transition(__VA_ARGS__)                                                    \
  }

/// Enables an epsiolon transition if the `statement` is true.
#define epsilon_if(statement, ...)                                             \
  if (statement) {                                                             \
    epsilon(__VA_ARGS__)                                                       \
  }

/// Enables an epsiolon transition if the constexpr `statement` is true.
#define epsilon_static_if(statement, ...)                                      \
  if constexpr (statement) {                                                   \
    epsilon(__VA_ARGS__)                                                       \
  }

/// Makes an transition transition into another FSM if `statement` is true,
/// resuming at state `target`.
#define fsm_transition_if(statement, ...)                                      \
  if (statement) {                                                             \
    fsm_transition(__VA_ARGS__)                                                \
  }

/// Makes an transition transition into another FSM if `statement` is true,
/// resuming at state `target`.
#define fsm_transition_static_if(statement, ...)                               \
  if constexpr (statement) {                                                   \
    fsm_transition(__VA_ARGS__)                                                \
  }

/// Makes an epsilon transition into another FSM if `statement` is true,
/// resuming at state `target`.
#define fsm_epsilon_if(statement, ...)                                         \
  if (statement) {                                                             \
    fsm_epsilon(__VA_ARGS__)                                                   \
  }

/// Makes an epsilon transition into another FSM if `statement` is true,
/// resuming at state `target`.
#define fsm_epsilon_static_if(statement, ...)                                  \
  if constexpr (statement) {                                                   \
    fsm_epsilon(__VA_ARGS__)                                                   \
  }
