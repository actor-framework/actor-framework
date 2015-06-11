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

#ifndef CAF_MESSAGE_ID_HPP
#define CAF_MESSAGE_ID_HPP

#include <cstdint>

#include "caf/config.hpp"
#include "caf/message_priority.hpp"
#include "caf/detail/comparable.hpp"

namespace caf {

struct invalid_message_id_t {
  constexpr invalid_message_id_t() {
    // nop
  }
};

constexpr invalid_message_id_t invalid_message_id = invalid_message_id_t{};

/// Denotes whether a message is asynchronous or synchronous
/// @note Asynchronous messages always have an invalid message id.
class message_id : detail::comparable<message_id> {
public:
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
    return valid() && ! is_response();
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

  static inline message_id from_integer_value(uint64_t value) {
    message_id result;
    result.value_ = value;
    return result;
  }

  static inline message_id make(message_priority prio
                                = message_priority::normal) {
    message_id res;
    return prio == message_priority::high ? res.with_high_priority() : res;
  }

  long compare(const message_id& other) const {
    return (value_ == other.value_) ? 0
                                      : (value_ < other.value_ ? -1 : 1);
  }

private:
  explicit inline message_id(uint64_t value) : value_(value) {
    // nop
  }
  uint64_t value_;
};

} // namespace caf

#endif // CAF_MESSAGE_ID_HPP
