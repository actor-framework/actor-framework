// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"
#include "caf/inspector_access.hpp"
#include "caf/message_priority.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace caf {

/// Bundles various flags along with an optional request ID.
class message_id : detail::comparable<message_id> {
public:
  // -- constants -------------------------------------------------------------

  /// The first bit flags response messages.
  static constexpr uint64_t response_flag_mask = 0x8000000000000000;

  /// The second bit flags whether the actor already responded.
  static constexpr uint64_t answered_flag_mask = 0x4000000000000000;

  // The third and fourth bit are used to categorize messages.
  static constexpr uint64_t category_flag_mask = 0x3000000000000000;

  /// The trailing 60 bits are used for the actual ID.
  static constexpr uint64_t request_id_mask = 0x0FFFFFFFFFFFFFFF;

  /// Identifies one-to-one messages with high priority.
  static constexpr uint64_t urgent_message_category = 0;

  /// Identifies one-to-one messages with normal priority.
  static constexpr uint64_t normal_message_category = 1;

  /// Number of bits trailing the category.
  static constexpr uint64_t category_offset = 60;

  /// Default value for asynchronous messages with normal message category.
  static constexpr uint64_t default_async_value = 0x1000000000000000;

  // -- constructors, destructors, and assignment operators -------------------

  /// Constructs a message ID for asynchronous messages with normal priority.
  constexpr message_id() : value_(default_async_value) {
    // nop
  }

  constexpr explicit message_id(uint64_t value) : value_(value) {
    // nop
  }

  message_id(const message_id&) = default;

  message_id& operator=(const message_id&) = default;

  // -- properties ------------------------------------------------------------

  /// Returns the message category, i.e., `normal_message_category` or
  /// `urgent_message_category`.
  constexpr uint64_t category() const noexcept {
    return (value_ & category_flag_mask) >> category_offset;
  }

  /// Returns a new message ID with given category.
  constexpr message_id with_category(uint64_t x) const noexcept {
    return message_id{(value_ & ~category_flag_mask) | (x << category_offset)};
  }

  /// Returns whether a message is asynchronous, i.e., not a request.
  constexpr bool is_async() const noexcept {
    return value_ == 0 || value_ == default_async_value;
  }

  /// Returns whether a message is a request.
  constexpr bool is_request() const noexcept {
    return (value_ & request_id_mask) != 0 && !is_response();
  }

  /// Returns whether a message is a response to a previously send request.
  constexpr bool is_response() const noexcept {
    return (value_ & response_flag_mask) != 0;
  }

  /// Returns whether a message is tagged as answered by the receiving actor.
  constexpr bool is_answered() const noexcept {
    return (value_ & answered_flag_mask) != 0;
  }

  /// Returns whether `category() == urgent_message_category`.
  constexpr bool is_urgent_message() const noexcept {
    return category() == urgent_message_category;
  }

  /// Returns whether `category() == normal_message_category`.
  constexpr bool is_normal_message() const noexcept {
    return category() == normal_message_category;
  }

  /// Returns the priority part from the `category()`.
  constexpr message_priority priority() const noexcept {
    return is_urgent_message() ? message_priority::high
                               : message_priority::normal;
  }

  /// Returns a response ID for the current request or an asynchronous ID with
  /// the same priority as this ID.
  constexpr message_id response_id() const noexcept {
    return is_request()
             ? message_id{value_ | response_flag_mask}
             : message_id{is_urgent_message() ? 0 : default_async_value};
  }

  /// Extracts the request number part of this ID.
  constexpr message_id request_id() const noexcept {
    return message_id{value_ & request_id_mask};
  }

  /// Returns the same ID but high message priority.
  constexpr message_id with_high_priority() const noexcept {
    return message_id{value_ & ~category_flag_mask};
  }

  /// Returns the same ID with normal message priority.
  constexpr message_id with_normal_priority() const noexcept {
    return message_id{value_ | default_async_value};
  }

  /// Returns the "raw bytes" for this ID.
  constexpr uint64_t integer_value() const noexcept {
    return value_;
  }

  /// Returns a negative value if `*this < other`, zero if `*this == other`,
  /// and a positive value otherwise.
  constexpr int64_t compare(const message_id& other) const noexcept {
    return static_cast<int64_t>(value_) - static_cast<int64_t>(other.value_);
  }

  /// Sets the flag for marking an incoming message as answered.
  void mark_as_answered() noexcept {
    value_ |= answered_flag_mask;
  }

  // -- operators -------------------------------------------------------------

  message_id& operator++() noexcept {
    ++value_;
    return *this;
  }

private:
  // -- member variables -------------------------------------------------------

  uint64_t value_;
};

// -- related free functions ---------------------------------------------------

/// Generates a `message_id` with given integer value.
/// @relates message_id
constexpr message_id make_message_id(normal_message_priority_constant,
                                     uint64_t value) {
  return message_id{value | message_id::default_async_value};
}

/// Generates a `message_id` with given integer value.
/// @relates message_id
constexpr message_id make_message_id(high_message_priority_constant,
                                     uint64_t value) {
  return message_id{value};
}

/// Generates a `message_id` with given integer value.
/// @relates message_id
template <message_priority P = message_priority::normal>
constexpr message_id make_message_id(uint64_t value = 0) {
  return make_message_id(std::integral_constant<message_priority, P>{}, value);
}

/// Generates a `message_id` with given priority.
/// @relates message_id
constexpr message_id make_message_id(message_priority p) {
  return message_id{static_cast<uint64_t>(p) << message_id::category_offset};
}

// -- inspection support -------------------------------------------------------

template <class Inspector>
bool inspect(Inspector& f, message_id& x) {
  auto get = [&x] { return x.integer_value(); };
  auto set = [&x](uint64_t val) {
    x = message_id{val};
    return true;
  };
  return f.apply(get, set);
}

} // namespace caf

namespace std {

template <>
struct hash<caf::message_id> {
  size_t operator()(caf::message_id x) const noexcept {
    hash<uint64_t> f;
    return f(x.integer_value());
  }
};

} // namespace std
