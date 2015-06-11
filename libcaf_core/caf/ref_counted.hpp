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

#ifndef CAF_REF_COUNTED_HPP
#define CAF_REF_COUNTED_HPP

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
  ~ref_counted();

  ref_counted();
  ref_counted(const ref_counted&);
  ref_counted& operator=(const ref_counted&);

  /// Increases reference count by one.
  inline void ref() noexcept {
    rc_.fetch_add(1, std::memory_order_relaxed);
  }

  /// Decreases reference count by one and calls `request_deletion`
  /// when it drops to zero.
  void deref() noexcept;

  /// Queries whether there is exactly one reference.
  inline bool unique() const noexcept {
    return rc_ == 1;
  }

  inline size_t get_reference_count() const noexcept {
    return rc_;
  }

protected:
  std::atomic<size_t> rc_;
};

/// @relates ref_counted
inline void intrusive_ptr_add_ref(ref_counted* p) {
  p->ref();
}

/// @relates ref_counted
inline void intrusive_ptr_release(ref_counted* p) {
  p->deref();
}

} // namespace caf

#endif // CAF_REF_COUNTED_HPP
