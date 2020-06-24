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
typename Inspector::result_type inspect(Inspector& f, new_round_result& x) {
  return f(meta::type_name("new_round_result"), x.consumed_items, x.stop_all);
}

} // namespace caf::intrusive
