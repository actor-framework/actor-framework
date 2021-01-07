// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail {

template <class First, typename Second>
struct type_pair {
  using first = First;
  using second = Second;
};

template <class First, typename Second>
struct to_type_pair {
  using type = type_pair<First, Second>;
};

template <class What>
struct is_type_pair {
  static constexpr bool value = false;
};

template <class First, typename Second>
struct is_type_pair<type_pair<First, Second>> {
  static constexpr bool value = true;
};

} // namespace caf::detail
