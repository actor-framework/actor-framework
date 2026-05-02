// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/any_ref.hpp"
#include "caf/detail/assert.hpp"
#include "caf/type_id.hpp"

#include <memory>

namespace caf {

/// Non-owning, read-only view to a single value.
///
/// @warning Must not outlive the referenced `any_ref` or its backing object.
class const_any_view {
public:
  explicit const_any_view(const detail::any_ref& ref) noexcept
    : ref_(std::addressof(ref)) {
  }

  type_id_t type_id() const noexcept {
    return ref_->type_id();
  }

  /// @pre `type_id() == type_id_v<T>`
  template <class T>
  const T& get() const noexcept {
    CAF_ASSERT(type_id() == type_id_v<T>);
    return *static_cast<const T*>(ref_->get());
  }

  template <class T>
  const T* get_if() const noexcept {
    if (ref_->type_id() == type_id_v<T>)
      return static_cast<const T*>(ref_->get());
    return nullptr;
  }

private:
  const detail::any_ref* ref_;
};

} // namespace caf
