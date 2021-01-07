// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail {

struct select_all {
  template <class T, class U>
  constexpr bool operator()(const T&, const U&) const noexcept {
    return true;
  }
};

} // namespace caf::detail
