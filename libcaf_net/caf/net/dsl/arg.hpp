// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/expected.hpp"

#include <optional>

namespace caf::net::dsl::arg {

/// Represents a null-terminated string or `null`.
class cstring {
public:
  cstring() : data_(nullptr) {
    // nop
  }

  cstring(const char* str) : data_(str) {
    // nop
  }

  cstring(std::string str) : data_(std::move(str)) {
    // nop
  }

  cstring(std::optional<const char*> str) : cstring() {
    if (str)
      data_ = *str;
  }

  cstring(std::optional<std::string> str) : cstring() {
    if (str)
      data_ = std::move(*str);
  }

  cstring(caf::expected<const char*> str) : cstring() {
    if (str)
      data_ = *str;
  }

  cstring(caf::expected<std::string> str) : cstring() {
    if (str)
      data_ = std::move(*str);
  }

  cstring(cstring&&) = default;

  cstring(const cstring&) = default;

  cstring& operator=(cstring&&) = default;

  cstring& operator=(const cstring&) = default;

  /// @returns a pointer to the null-terminated string.
  const char* get() const noexcept {
    return std::visit(
      [](auto& arg) -> const char* {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, const char*>) {
          return arg;
        } else {
          return arg.c_str();
        }
      },
      data_);
  }

  bool has_value() const noexcept {
    return !operator!();
  }

  explicit operator bool() const noexcept {
    return has_value();
  }

  bool operator!() const noexcept {
    return data_.index() == 0 && std::get<0>(data_) == nullptr;
  }

private:
  std::variant<const char*, std::string> data_;
};

/// Represents a value of type T or `null`.
template <class T>
class val {
public:
  val() = default;

  val(T value) : data_(std::move(value)) {
    // nop
  }

  val(std::optional<T> value) : data_(std::move(value)) {
    // nop
  }

  val(caf::expected<T> value) {
    if (value)
      data_ = std::move(*value);
  }

  val(val&&) = default;

  val(const val&) = default;

  val& operator=(val&&) = default;

  val& operator=(const val&) = default;

  const T& get() const noexcept {
    return *data_; // NOLINT(bugprone-unchecked-optional-access)
  }

  explicit operator bool() const noexcept {
    return data_.has_value();
  }

  bool operator!() const noexcept {
    return !data_;
  }

private:
  std::optional<T> data_;
};

} // namespace caf::net::dsl::arg
