// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/pec.hpp"

#include <optional>
#include <type_traits>
#include <utility>

namespace caf::detail {

/// Consumes a single value.
template <class T>
class consumer {
public:
  using value_type = T;

  explicit consumer(T& x) : x_(x) {
    // nop
  }

  template <class U>
  void value(U&& y) {
    x_ = std::forward<U>(y);
  }

private:
  T& x_;
};

/// Specializes `consumer` for `int64_t` with a safe conversion from `uint64_t`.
template <>
class consumer<int64_t> {
public:
  using value_type = int64_t;

  explicit consumer(int64_t& x) : x_(x) {
    // nop
  }

  void value(int64_t y) noexcept {
    x_ = y;
  }

  pec value(uint64_t y) noexcept {
    if (y < INT64_MAX) {
      value(static_cast<int64_t>(y));
      return pec::success;
    }
    return pec::integer_overflow;
  }

private:
  int64_t& x_;
};

/// Specializes `consumer` for `uint64_t` with a safe conversion from `int64_t`.
template <>
class consumer<uint64_t> {
public:
  using value_type = uint64_t;

  explicit consumer(uint64_t& x) : x_(x) {
    // nop
  }

  void value(uint64_t y) noexcept {
    x_ = y;
  }

  pec value(int64_t y) noexcept {
    if (y >= 0) {
      value(static_cast<uint64_t>(y));
      return pec::success;
    }
    return pec::integer_underflow;
  }

private:
  uint64_t& x_;
};

template <class... Ts>
class consumer<std::variant<Ts...>> {
public:
  explicit consumer(std::variant<Ts...>& x) : x_(x) {
    // nop
  }

  template <class T>
  void value(T&& y) {
    x_ = std::forward<T>(y);
  }

private:
  std::variant<Ts...>& x_;
};

template <class T>
class consumer<std::optional<T>> {
public:
  using value_type = T;

  explicit consumer(std::optional<T>& x) : x_(x) {
    // nop
  }

  template <class U>
  void value(U&& y) {
    x_ = std::forward<U>(y);
  }

private:
  std::optional<T>& x_;
};

template <class T>
consumer<T> make_consumer(T& x) {
  return consumer<T>{x};
}

/// Applies a consumer to a value and updates the error code if necessary.
template <class Consumer, class T>
static void apply_consumer(Consumer&& consumer, T&& value, pec& code) {
  using res_t = decltype(consumer.value(std::forward<T>(value)));
  if constexpr (std::is_same_v<res_t, void>) {
    consumer.value(std::forward<T>(value));
  } else {
    auto res = consumer.value(std::forward<T>(value));
    if (res != pec::success)
      code = res;
  }
}

} // namespace caf::detail
