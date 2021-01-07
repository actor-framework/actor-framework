// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/detail/type_traits.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"

namespace caf {

/// Constructs an object of type `T` in an `intrusive_ptr`.
/// @relates ref_counted
template <class T, class... Ts>
intrusive_ptr<T> make_counted(Ts&&... xs) {
  return intrusive_ptr<T>(new T(std::forward<Ts>(xs)...), false);
}

} // namespace caf
