/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#pragma once

#include <type_traits>

#include "caf/fwd.hpp"

namespace caf {
namespace intrusive {

/// Returns the state of a consumer from `new_round`.
struct new_round_result {
  /// Denotes whether the consumer accepted at least one element.
  bool consumed_items : 1;
  /// Denotes whether the consumer returned `task_result::stop_all`.
  bool stop_all : 1;
};

constexpr bool operator==(new_round_result x, new_round_result y) {
  return x.consumed_items == y.consumed_items && x.stop_all == y.stop_all;
}

constexpr bool operator!=(new_round_result x, new_round_result y) {
  return !(x == y);
}

constexpr new_round_result make_new_round_result(bool consumed_items,
                                                 bool stop_all = false) {
  return {consumed_items, stop_all};
}

constexpr new_round_result operator|(new_round_result x, new_round_result y) {
  return {x.consumed_items || y.consumed_items, x.stop_all || y.stop_all};
}

} // namespace intrusive
} // namespace caf

