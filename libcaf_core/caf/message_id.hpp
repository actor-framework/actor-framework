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

/// Denotes whether a message is asynchronous or synchronous.
/// @note Asynchronous messages always have an invalid message id.
class message_id : detail::comparable<message_id> {
public:
  // -- friends ---------------------------------------------------------------

  friend struct make_message_id_t;

  // -- constants -------------------------------------------------------------

  /// The first bit flags response messages.
  static constexpr uint64_t response_flag_mask = 0x8000000000000000;

  /// The second bit flags whether the actor already responded.
  static constexpr uint64_t answered_flag_mask = 0x4000000000000000;

  // The third and fourth bit are used to categorize messages.
  static constexpr uint64_t category_flag_mask = 0x3000000000000000;

  /// The trailing 60 bits are used for the actual ID.
  static constexpr uint64_t request_id_mask = 0x0FFFFFFFFFFFFFFF;

  /// Identifies one-to-one messages with normal priority.
  static constexpr uint64_t default_message_category = 0; // 0b00

  /// Identifies stream messages that flow upstream, e.g.,
  /// `upstream_msg::ack_batch`.
  static constexpr uint64_t upstream_message_category = 1; // 0b01

  /// Identifies stream messages that flow downstream, e.g.,
  /// `downstream_msg::batch`.
  static constexpr uint64_t downstream_message_category = 2; // 0b10

  /// Identifies one-to-one messages with high priority.
  static constexpr uint64_t urgent_message_category = 3; // 0b11

  /// Number of bits trailing the category.
  static constexpr uint64_t category_offset = 60;

  // -- constructors, destructors, and assignment operators -------------------

  /// Constructs a message ID for asynchronous messages with normal priority.
  constexpr message_id() : value_(0) {
    // nop
  }

  /// Constructs a message ID for asynchronous messages with normal priority.
  constexpr message_id(invalid_message_id_t) : message_id() {
    // nop
  }

  message_id(const message_id&) = default;

  message_id& operator=(const message_id&) = default;

  // -- observers -------------------------------------------------------------

  /// Returns whether a message is asynchronous, i.e., neither a request nor a
  /// stream message.
  inline bool is_async() const {
    return value_ == 0
           || value_ == (urgent_message_category << category_offset);
  }

  /// Returns whether a message is a response to a previously send request.
  inline bool is_response() const {
    return (value_ & response_flag_mask) != 0;
  }

  /// Returns whether a message is a request.
  inline bool is_request() const {
    return valid() && !is_response();
  }

  /// Returns whether a message is tagged as answered by the receiving actor.
  inline bool is_answered() const {
    return (value_ & answered_flag_mask) != 0;
  }

  /// Returns the message category, i.e., one of `default_message_category`,
  /// `upstream_message_category`, `downstream_message_category`, or
  /// `urgent_message_category`.
  inline uint64_t category() const {
    return (value_ & category_flag_mask) >> category_offset;
  }

  /// Returns whether `category() == default_message_category`.
  inline bool is_default_message() const {
    return (value_ & category_flag_mask) == 0;
  }

  /// Returns whether `category() == urgent_message_category`.
  inline bool is_urgent_message() const {
    return (value_ & category_flag_mask)
           == (urgent_message_category << category_offset);
  }

  /// Returns whether `category() == upstream_message_category`.
  inline bool is_upstream_message() const {
    return (value_ & category_flag_mask)
           == (urgent_message_category << category_offset);
  }

  /// Returns whether `category() == downstream_message_category`.
  inline bool is_downstream_message() const {
    return (value_ & category_flag_mask)
           == (urgent_message_category << category_offset);
  }

  /// Returns whether a message is a request or a response.
  inline bool valid() const {
    return (value_ & request_id_mask) != 0;
  }

  /// Returns a response ID for the current request or an asynchronous ID with
  /// the same priority as this ID.
  inline message_id response_id() const {
    if (is_request())
      return message_id{value_ | response_flag_mask};
    return message_id{is_urgent_message()
                      ? urgent_message_category << category_offset
                      : 0u};
  }

  /// Extracts the request number part of this ID.
  inline message_id request_id() const {
    return message_id{value_ & request_id_mask};
  }

  /// Returns the same ID but with high instead of default message priority.
  /// @pre `!is_upstream_message() && !is_downstream_message()`
  inline message_id with_high_priority() const {
    return message_id{value_ | category_flag_mask}; // works because urgent == 0b11
  }

  /// Returns the same ID but with high instead of default message priority.
  inline message_id with_normal_priority() const {
    return message_id{value_ & ~category_flag_mask}; // works because normal == 0b00
  }

  /// Returns the "raw bytes" for this ID.
  inline uint64_t integer_value() const {
    return value_;
  }

  /// Returns a negative value if `*this < other`, zero if `*this == other`,
  /// and a positive value otherwise.
  inline int64_t compare(const message_id& other) const {
    return static_cast<int64_t>(value_) - static_cast<int64_t>(other.value_);
  }

  // -- mutators --------------------------------------------------------------

  inline message_id& operator++() {
    ++value_;
    return *this;
  }

  /// Sets the flag for marking an incoming message as answered.
  inline void mark_as_answered() {
    value_ |= answered_flag_mask;
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
  constexpr explicit message_id(uint64_t value) : value_(value) {
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
    return message_id{value};
  }

  constexpr message_id operator()(message_priority p) const {
    return message_id{static_cast<uint64_t>(p) << message_id::category_offset};
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

