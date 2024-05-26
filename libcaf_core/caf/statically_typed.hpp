// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <caf/type_list.hpp>

namespace caf {

/// A statically typed trait type wrapping the given parameter pack of
/// signatures as a type list.
/// @note used for backwards compatibility when declaring typed interfaces.
template <class... Sigs>
struct statically_typed {
  using signatures = type_list<Sigs...>;
};

} // namespace caf
