// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

/// Policy for adding and releasing references in an @ref intrusive_ptr. The
/// default implementation dispatches to the free function pair
/// `intrusive_ptr_add_ref` and `intrusive_ptr_release` that the policy picks up
/// via ADL.
/// @relates intrusive_ptr
template <class T>
struct intrusive_ptr_access {
public:
  static void add_ref(T* ptr) noexcept {
    intrusive_ptr_add_ref(ptr);
  }

  static void release(T* ptr) noexcept {
    intrusive_ptr_release(ptr);
  }
};

} // namespace caf
