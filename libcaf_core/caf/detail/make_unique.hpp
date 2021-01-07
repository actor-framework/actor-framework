// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <memory>

namespace caf::detail {

template <class T, class... Ts>
std::unique_ptr<T> make_unique(Ts&&... xs) {
  return std::unique_ptr<T>{new T(std::forward<Ts>(xs)...)};
}

} // namespace caf::detail
