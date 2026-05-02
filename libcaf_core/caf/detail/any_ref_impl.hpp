// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/any_ref.hpp"
#include "caf/type_id.hpp"

#include <memory>
#include <type_traits>

namespace caf::detail {

/// Wraps a reference to `T` as an `any_ref`. `T` must have a CAF type ID.
template <class T>
class any_ref_impl final : public any_ref {
public:
  static_assert(has_type_id_v<T>, "T must be an announced type");

  static_assert(!std::is_const_v<T>);

  explicit any_ref_impl(T& value) noexcept : value_(std::addressof(value)) {
    // nop
  }

  type_id_t type_id() const noexcept override {
    return type_id_v<T>;
  }

  void* get() noexcept override {
    return value_;
  }

  const void* get() const noexcept override {
    return value_;
  }

private:
  T* value_;
};

template <class T>
any_ref_impl(T&) -> any_ref_impl<T>;

} // namespace caf::detail
