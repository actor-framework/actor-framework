// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/intrusive_cow_ptr.hpp"

namespace caf {

/// Constructs an object of type `T` in an `intrusive_cow_ptr`.
/// @relates intrusive_cow_ptr
template <class T, class... Ts>
intrusive_cow_ptr<T> make_copy_on_write(Ts&&... xs) {
  return intrusive_cow_ptr<T>(new T(std::forward<Ts>(xs)...), false);
}

} // namespace caf
