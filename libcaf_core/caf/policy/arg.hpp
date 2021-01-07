// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::policy {

/// Provides a wrapper to pass policy types as values to functions.
template <class... Ts>
struct arg {
public:
  static const arg value;
};

template <class... Ts>
const arg<Ts...> arg<Ts...>::value = arg<Ts...>{};

} // namespace caf::policy
