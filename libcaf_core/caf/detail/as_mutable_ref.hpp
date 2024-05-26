// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

// This function performs a const_cast. Naturally, calling it is almost always a
// very bad idea. With one notable exception: writing code for type inspection.

namespace caf::detail {

template <class T>
T& as_mutable_ref(T& x) {
  return x;
}

template <class T>
T& as_mutable_ref(const T& x) {
  return const_cast<T&>(x);
}

} // namespace caf::detail
