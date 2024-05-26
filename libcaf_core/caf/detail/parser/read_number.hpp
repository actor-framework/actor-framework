// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/consumer.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/is_digit.hpp"
#include "caf/detail/parser/read_floating_point.hpp"
#include "caf/detail/parser/sub_ascii.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/pec.hpp"

#include <cstdint>
#include <type_traits>

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

/// Reads the second half of 'n..m' range statement.
///
/// Expect the current position to point at the number *after* the dots:
///
/// ~~~
/// foo = [1..2]
///        ~~~^
/// ~~~
template <class State, class Consumer, class ValueType>
void read_number_range(State& ps, Consumer& consumer, ValueType begin);

template <class State, class Consumer, class EnableFloat = std::true_type,
          class EnableRange = std::false_type>
void read_negative_number(State& ps, Consumer& consumer, EnableFloat = {},
                          EnableRange = {}) {
  constexpr bool enable_float = EnableFloat::value;
  constexpr bool enable_range = EnableRange::value;
  // Our result when reading an integer number.
  auto result = int64_t{0};
  auto disabled = false;
  using odbl = std::optional<double>;
  // clang-format off
  start();
  // Initial state.
  state(init) {
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer, odbl{0.}, true),
                          done, '.', disabled = true)
    transition(neg_zero, '0')
    epsilon(neg_dec)
  }
  // Disambiguate base.
  term_state(neg_zero) {
    transition(start_neg_bin, "bB")
    transition(start_neg_hex, "xX")
    transition_static_if(enable_float || enable_range, neg_dot, '.')
    epsilon(neg_oct)
  }
  // Binary integers.
  state(start_neg_bin) {
    epsilon(neg_bin)
  }
  term_state(neg_bin) {
    transition(neg_bin, "01", sub_ascii<2>(result, ch), pec::integer_underflow)
  }
  // Octal integers.
  state(start_neg_oct) {
    epsilon(neg_oct)
  }
  term_state(neg_oct) {
    transition(neg_oct, octal_chars, sub_ascii<8>(result, ch),
               pec::integer_underflow)
  }
  // Hexal integers.
  state(start_neg_hex) {
    epsilon(neg_hex)
  }
  term_state(neg_hex) {
    transition(neg_hex, hexadecimal_chars, sub_ascii<16>(result, ch),
               pec::integer_underflow)
  }
  // Reads the integer part of the mantissa or a negative decimal integer.
  term_state(neg_dec) {
    transition(neg_dec, decimal_chars, sub_ascii<10>(result, ch),
               pec::integer_underflow)
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer,
                                      odbl{static_cast<double>(result)}, true),
                          done, "eE", disabled = true)
    transition_static_if(enable_float || enable_range, neg_dot, '.')
  }
  unstable_state(neg_dot) {
    fsm_transition_static_if(enable_range,
                             read_number_range(ps, consumer, result),
                             done, '.', disabled = true)
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer,
                                      odbl{static_cast<double>(result)}, true),
                          done, any_char, disabled = true)
    epsilon(done)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
  if (!disabled && ps.code <= pec::trailing_character)
    apply_consumer(consumer, result, ps.code);
}

template <class State, class Consumer, class EnableFloat = std::true_type,
          class EnableRange = std::false_type>
void read_positive_number(State& ps, Consumer& consumer, EnableFloat = {},
                          EnableRange = {}) {
  constexpr bool enable_float = EnableFloat::value;
  constexpr bool enable_range = EnableRange::value;
  // Our result when reading an integer number.
  auto result = uint64_t{0};
  auto disabled = false;
  using odbl = std::optional<double>;
  // clang-format off
  start();
  // Initial state.
  state(init) {
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer, odbl{0.}),
                          done, '.', disabled = true)
    transition(pos_zero, '0')
    epsilon(pos_dec)
  }
  // Disambiguate base.
  term_state(pos_zero) {
    transition(start_pos_bin, "bB")
    transition(start_pos_hex, "xX")
    transition_static_if(enable_float || enable_range, pos_dot, '.')
    epsilon(pos_oct)
  }
  // Binary integers.
  state(start_pos_bin) {
    epsilon(pos_bin)
  }
  term_state(pos_bin) {
    transition(pos_bin, "01", add_ascii<2>(result, ch), pec::integer_overflow)
  }
  // Octal integers.
  state(start_pos_oct) {
    epsilon(pos_oct)
  }
  term_state(pos_oct) {
    transition(pos_oct, octal_chars, add_ascii<8>(result, ch),
               pec::integer_overflow)
  }
  // Hexal integers.
  state(start_pos_hex) {
    epsilon(pos_hex)
  }
  term_state(pos_hex) {
    transition(pos_hex, hexadecimal_chars, add_ascii<16>(result, ch),
               pec::integer_overflow)
  }
  // Reads the integer part of the mantissa or a positive decimal integer.
  term_state(pos_dec) {
    transition(pos_dec, decimal_chars, add_ascii<10>(result, ch),
               pec::integer_overflow)
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer,
                                      odbl{static_cast<double>(result)}),
                          done, "eE", disabled = true)
    transition_static_if(enable_float || enable_range, pos_dot, '.')
  }
  // Reads the integer part of the mantissa or a negative decimal integer.
  unstable_state(pos_dot) {
    fsm_transition_static_if(enable_range,
                             read_number_range(ps, consumer, result),
                             done, '.', disabled = true)
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer,
                                      odbl{static_cast<double>(result)}),
                          done, any_char, disabled = true)
    epsilon(done)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
  if (!disabled && ps.code <= pec::trailing_character)
    apply_consumer(consumer, result, ps.code);
}

/// Reads a number, i.e., on success produces an `int64_t`, an `uint64_t` or a
/// `double`.
template <class State, class Consumer, class EnableFloat = std::true_type,
          class EnableRange = std::false_type>
void read_number(State& ps, Consumer& consumer, EnableFloat fl_token = {},
                 EnableRange rng_token = {}) {
  constexpr bool enable_float = EnableFloat::value;
  using odbl = std::optional<double>;
  // clang-format off
  // Definition of our parser FSM.
  start();
  state(init) {
    transition(init, " \t")
    fsm_transition(read_positive_number(ps, consumer, fl_token, rng_token),
                   done, '+')
    fsm_transition(read_negative_number(ps, consumer, fl_token, rng_token),
                   done, '-')
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer, odbl{0.}),
                          done, '.')
    fsm_epsilon(read_positive_number(ps, consumer, fl_token, rng_token), done)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

/// Generates a range of numbers and calls `consumer` for each value.
template <class Consumer, class T>
void generate_range_impl(pec& code, Consumer& consumer, T min_val, T max_val,
                         std::optional<int64_t> step) {
  auto do_apply = [&](T x) {
    using consumer_result_type = decltype(consumer.value(x));
    if constexpr (std::is_same_v<consumer_result_type, pec>) {
      auto res = consumer.value(x);
      if (res == pec::success)
        return true;
      code = res;
      return false;
    } else {
      static_assert(std::is_same_v<consumer_result_type, void>);
      consumer.value(x);
      return true;
    }
  };
  if (min_val == max_val) {
    do_apply(min_val);
    return;
  }
  if (min_val < max_val) {
    auto step_val = step.value_or(1);
    if (step_val <= 0) {
      code = pec::invalid_range_expression;
      return;
    }
    auto s = static_cast<T>(step_val);
    auto i = min_val;
    while (i < max_val) {
      if (!do_apply(i))
        return;
      if (max_val - i < s) // protect against overflows
        return;
      i += s;
    }
    if (i == max_val)
      do_apply(i);
    return;
  }
  auto step_val = step.value_or(-1);
  if (step_val >= 0) {
    code = pec::invalid_range_expression;
    return;
  }
  auto s = static_cast<T>(-step_val);
  auto i = min_val;
  while (i > max_val) {
    if (!do_apply(i))
      return;
    if (i - max_val < s) // protect against underflows
      return;
    i -= s;
  }
  if (i == max_val)
    do_apply(i);
}

/// Generates a range of numbers and calls `consumer` for each value.
template <class Consumer, class MinValueT, class MaxValueT>
void generate_range(pec& code, Consumer& consumer, MinValueT min_val,
                    MaxValueT max_val, std::optional<int64_t> step) {
  static_assert(is_64bit_integer_v<MinValueT>);
  static_assert(is_64bit_integer_v<MaxValueT>);
  // Check whether any of the two types is signed. If so, we'll use signed
  // integers for the range.
  if constexpr (std::is_signed_v<MinValueT> == std::is_signed_v<MaxValueT>) {
    generate_range_impl(code, consumer, min_val, max_val, step);
  } else if constexpr (std::is_signed_v<MinValueT>) {
    if (max_val > INT64_MAX)
      code = pec::integer_overflow;
    else
      generate_range_impl(code, consumer, min_val,
                          static_cast<int64_t>(max_val), step);
  } else {
    if (min_val > INT64_MAX)
      code = pec::integer_overflow;
    else
      generate_range_impl(code, consumer, static_cast<int64_t>(min_val),
                          max_val, step);
  }
}

template <class State, class Consumer, class ValueType>
void read_number_range(State& ps, Consumer& consumer, ValueType begin) {
  // Our final value (inclusive). We don't know yet whether we're dealing with
  // a signed or unsigned range.
  std::variant<none_t, int64_t, uint64_t> end;
  // Note: The step value is always signed, even if the range is unsigned. For
  // example, [10..2..-2] is a valid range description for the unsigned values
  // [10, 8, 6, 4, 2].
  std::optional<int64_t> step;
  auto end_consumer = make_consumer(end);
  auto step_consumer = make_consumer(step);
  constexpr auto no_float = std::false_type{};
  // clang-format off
  // Definition of our parser FSM.
  start();
  state(init) {
    fsm_epsilon(read_number(ps, end_consumer, no_float), after_end_num)
  }
  term_state(after_end_num) {
    transition(first_dot, '.')
  }
  state(first_dot) {
    transition(second_dot, '.')
  }
  state(second_dot) {
    fsm_epsilon(read_number(ps, step_consumer, no_float), done)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
  if (ps.code > pec::trailing_character)
    return;
  auto fn = [&](auto end_val) {
    if constexpr (std::is_same_v<decltype(end_val), none_t>) {
      ps.code = pec::invalid_range_expression;
    } else {
      generate_range(ps.code, consumer, begin, end_val, step);
    }
  };
  std::visit(fn, end);
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
