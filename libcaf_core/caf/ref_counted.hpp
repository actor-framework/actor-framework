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

#include <atomic>
#include <cstddef>

#include "caf/memory_managed.hpp"

namespace caf {

/// Base class for reference counted objects with an atomic reference count.
/// Serves the requirements of {@link intrusive_ptr}.
/// @note *All* instances of `ref_counted` start with a reference count of 1.
/// @relates intrusive_ptr
class ref_counted : public memory_managed {
public:
  ~ref_counted() override;

  ref_counted();
  ref_counted(const ref_counted&);
  ref_counted& operator=(const ref_counted&);

  /// Increases reference count by one.
  inline void ref() const noexcept {
    rc_.fetch_add(1, std::memory_order_relaxed);
  }

  /// Decreases reference count by one and calls `request_deletion`
  /// when it drops to zero.
  void deref() const noexcept;

  /// Queries whether there is exactly one reference.
  inline bool unique() const noexcept {
    return rc_ == 1;
  }

  inline size_t get_reference_count() const noexcept {
    return rc_;
  }

protected:
  mutable std::atomic<size_t> rc_;
};

/// @relates ref_counted
inline void intrusive_ptr_add_ref(const ref_counted* p) {
  p->ref();
}

/// @relates ref_counted
inline void intrusive_ptr_release(const ref_counted* p) {
  p->deref();
}

} // namespace caf

