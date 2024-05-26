// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <type_traits>

namespace caf {

/// Helper class to indicate that a request has been forwarded.
template <class... Ts>
class delegated {
  // nop
};

template <class... Ts>
bool operator==(const delegated<Ts...>&, const delegated<Ts...>&) {
  return true;
}

} // namespace caf
