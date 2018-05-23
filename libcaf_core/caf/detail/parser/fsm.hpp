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

#define CAF_FSM_EVAL_MISMATCH_EC                                               \
  if (mismatch_ec == ec::unexpected_character)                                 \
    ps.code = ch != '\n' ? mismatch_ec : ec::unexpected_newline;               \
  else                                                                         \
    ps.code = mismatch_ec;                                                     \
  return;

#define start()                                                                \
  char ch = ps.current();                                                      \
  goto s_init;                                                                 \
  s_unexpected_eof:                                                            \
  ps.code = ec::unexpected_eof;                                                \
  return;                                                                      \
  {                                                                            \
    static constexpr ec mismatch_ec = ec::unexpected_character

#define state(name)                                                            \
  CAF_FSM_EVAL_MISMATCH_EC                                                     \
  }                                                                            \
  {                                                                            \
    static constexpr ec mismatch_ec = ec::unexpected_character;                \
    s_##name : __attribute__((unused));                                        \
    if (ch == '\0')                                                            \
      goto s_unexpected_eof;                                                   \
    e_##name : __attribute__((unused));

#define term_state(name)                                                       \
  CAF_FSM_EVAL_MISMATCH_EC                                                     \
  }                                                                            \
  {                                                                            \
    static constexpr ec mismatch_ec = ec::trailing_character;                  \
    s_##name : __attribute__((unused));                                        \
    if (ch == '\0')                                                            \
      goto s_fin;                                                              \
    e_##name : __attribute__((unused));

#define fin()                                                                  \
  CAF_FSM_EVAL_MISMATCH_EC                                                     \
  }                                                                            \
  s_fin:                                                                       \
  ps.code = ec::success;                                                       \
  return;

#define input(predicate, target)                                               \
  if (predicate(ch)) {                                                         \
    ch = ps.next();                                                            \
    goto s_##target;                                                           \
  }

#define action(predicate, target, statement)                                   \
  if (predicate(ch)) {                                                         \
    statement;                                                                 \
    ch = ps.next();                                                            \
    goto s_##target;                                                           \
  }

#define default_action(target, statement)                                      \
  statement;                                                                   \
  ch = ps.next();                                                              \
  goto s_##target;

#define checked_action(predicate, target, statement, error_code)               \
  if (predicate(ch)) {                                                         \
    if (statement) {                                                           \
      ch = ps.next();                                                          \
      goto s_##target;                                                         \
    } else {                                                                   \
      ps.code = error_code;                                                    \
      return;                                                                  \
    }                                                                          \
  }

// Unconditionally jumps to target for any input.
#define any_input(target)                                                      \
  ch = ps.next();                                                              \
  goto s_##target;

#define invalid_input(predicate, error_code)                                   \
  if (predicate(ch)) {                                                         \
    ps.code = error_code;                                                      \
    return;                                                                    \
  }

#define default_failure(error_code)                                            \
  ps.code = error_code;                                                        \
  return;

// Makes an unconditional epsiolon transition into another state.
#define epsilon(target) goto e_##target;

// Makes an epsiolon transition into another state if the `statement` is true.
#define checked_epsilon(statement, target)                                     \
  if (statement)                                                               \
    goto e_##target;

// Transitions into the `target` FSM.
#define invoke_fsm(fsm_call, target)                                           \
  fsm_call;                                                                    \
  if (ps.code > ec::trailing_character)                                        \
    return;                                                                    \
  ch = ps.current();                                                           \
  goto s_##target;

#define invoke_fsm_if(predicate, fsm_call, target)                             \
  if (predicate(ch)) {                                                         \
    fsm_call;                                                                  \
    if (ps.code > ec::trailing_character)                                      \
      return;                                                                  \
    ch = ps.current();                                                         \
    goto s_##target;                                                           \
  }

