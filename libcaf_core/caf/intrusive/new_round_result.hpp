// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/meta/type_name.hpp"

namespace caf::intrusive {

/// Returns the state of a consumer from `new_round`.
struct new_round_result {
  /// Denotes whether the consumer accepted at least one element.
  size_t consumed_items;
  /// Denotes whether the consumer returned `task_result::stop_all`.
  bool stop_all;
};

constexpr bool operator==(new_round_result x, new_round_result y) {
  return x.consumed_items == y.consumed_items && x.stop_all == y.stop_all;
}

constexpr bool operator!=(new_round_result x, new_round_result y) {
  return !(x == y);
}

template <class Inspector>
bool inspect(Inspector& f, new_round_result& x) {
  return f.object(x).fields(f.field("consumed_items", x.consumed_items),
                            f.field("stop_all", x.stop_all));
}

} // namespace caf::intrusive
