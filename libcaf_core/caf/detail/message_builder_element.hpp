// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/padded_size.hpp"

#include <cstddef>
#include <memory>
#include <new>

namespace caf::detail {

/// Wraps a value for either copying or moving it into a pre-allocated storage
/// later.
class CAF_CORE_EXPORT message_builder_element {
public:
  message_builder_element() = default;
  message_builder_element(const message_builder_element&) = delete;
  message_builder_element& operator=(const message_builder_element&) = delete;

  virtual ~message_builder_element();

  /// Uses placement new to create a copy of the wrapped value at given memory
  /// region.
  /// @returns the past-the-end pointer of the object, i.e., the first byte
  /// for
  ///          the *next* object.
  virtual std::byte* copy_init(std::byte* storage) const = 0;

  /// Uses placement new to move the wrapped value to given memory region.
  /// @returns the past-the-end pointer of the object, i.e., the first byte
  /// for
  ///          the *next* object.
  virtual std::byte* move_init(std::byte* storage) = 0;
};

template <class T>
class message_builder_element_impl : public message_builder_element {
public:
  template <class Value>
  explicit message_builder_element_impl(Value&& value)
    : value_(std::forward<Value>(value)) {
    // nop
  }

  message_builder_element_impl() = delete;

  message_builder_element_impl(message_builder_element_impl&&) = delete;

  message_builder_element_impl(const message_builder_element_impl&) = delete;

  message_builder_element_impl& operator=(message_builder_element_impl&&)
    = delete;

  message_builder_element_impl& operator=(const message_builder_element_impl&)
    = delete;

  std::byte* copy_init(std::byte* storage) const override {
    new (storage) T(value_);
    return storage + padded_size_v<T>;
  }

  std::byte* move_init(std::byte* storage) override {
    new (storage) T(std::move(value_));
    return storage + padded_size_v<T>;
  }

private:
  T value_;
};

using message_builder_element_ptr = std::unique_ptr<message_builder_element>;

} // namespace caf::detail
