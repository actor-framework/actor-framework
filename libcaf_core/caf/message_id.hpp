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

#ifndef CAF_MESSAGE_ID_HPP
#define CAF_MESSAGE_ID_HPP

#include <string>
#include <cstdint>
#include <functional>

#include "caf/config.hpp"
#include "caf/error.hpp"
#include "caf/message_priority.hpp"

#include "caf/meta/type_name.hpp"

#include "caf/detail/comparable.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

struct invalid_message_id_t {
  constexpr invalid_message_id_t() {
    // nop
  }
};

constexpr invalid_message_id_t invalid_message_id = invalid_message_id_t{};

struct make_message_id_t;

/// Denotes whether a message is asynchronous or synchronous
/// @note Asynchronous messages always have an invalid message id.
class message_id : detail::comparable<message_id> {
public:
  // -- friends ---------------------------------------------------------------

  friend struct make_message_id_t;

  // -- constants -------------------------------------------------------------

  static constexpr uint64_t response_flag_mask = 0x8000000000000000;
  static constexpr uint64_t answered_flag_mask = 0x4000000000000000;
  static constexpr uint64_t high_prioity_flag_mask = 0x2000000000000000;
  static constexpr uint64_t request_id_mask = 0x1FFFFFFFFFFFFFFF;

  constexpr message_id() : value_(0) {
    // nop
  }

  constexpr message_id(invalid_message_id_t) : value_(0) {
    // nop
  }

  message_id(message_id&&) = default;
  message_id(const message_id&) = default;
  message_id& operator=(message_id&&) = default;
  message_id& operator=(const message_id&) = default;

  inline message_id& operator++() {
    ++value_;
    return *this;
  }

  inline bool is_async() const {
    return value_ == 0 || value_ == high_prioity_flag_mask;
  }

  inline bool is_response() const {
    return (value_ & response_flag_mask) != 0;
  }

  inline bool is_answered() const {
    return (value_ & answered_flag_mask) != 0;
  }

  inline bool is_high_priority() const {
    return (value_ & high_prioity_flag_mask) != 0;
  }

  inline bool valid() const {
    return (value_ & request_id_mask) != 0;
  }

  inline bool is_request() const {
    return valid() && !is_response();
  }

  inline message_id response_id() const {
    return message_id{is_request() ? value_ | response_flag_mask : 0};
  }

  inline message_id request_id() const {
    return message_id(value_ & request_id_mask);
  }

  inline message_id with_high_priority() const {
    return message_id(value_ | high_prioity_flag_mask);
  }

  inline message_id with_normal_priority() const {
    return message_id(value_ & ~high_prioity_flag_mask);
  }

  inline void mark_as_answered() {
    value_ |= answered_flag_mask;
  }

  inline uint64_t integer_value() const {
    return value_;
  }

  long compare(const message_id& other) const {
    return (value_ == other.value_) ? 0
                                      : (value_ < other.value_ ? -1 : 1);
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f, message_id& x) {
    return f(meta::type_name("message_id"), x.value_);
  }

  // -- deprecated functions ---------------------------------------------------

  template <class... Ts>
  static message_id make(Ts&&... xs)
  CAF_DEPRECATED_MSG("use make_message_id instead");

private:
  constexpr message_id(uint64_t value) : value_(value) {
    // nop
  }

  uint64_t value_;
};

// -- make_message_id ----------------------------------------------------------

/// Utility class for generating a `message_id` from integer values or
/// priorities.
struct make_message_id_t {
  constexpr make_message_id_t() {
    // nop
  }

  constexpr message_id operator()(uint64_t value = 0) const {
    return value;
  }

  constexpr message_id operator()(message_priority p) const {
    return p == message_priority::high ? message_id::high_prioity_flag_mask
                                       : 0u;
  }
};

/// Generates a `message_id` from integer values or priorities.
constexpr make_message_id_t make_message_id = make_message_id_t{};

// -- deprecated functions -----------------------------------------------------

template <class... Ts>
message_id message_id::make(Ts&&... xs) {
  return make_message_id(std::forward<Ts>(xs)...);
}

} // namespace caf

namespace std {

template <>
struct hash<caf::message_id> {
  inline size_t operator()(caf::message_id x) const {
    hash<uint64_t> f;
    return f(x.integer_value());
  }
};

} // namespace std

#endif // CAF_MESSAGE_ID_HPP
