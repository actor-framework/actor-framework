// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/statically_typed.hpp"

#include <type_traits>

namespace caf::detail {

template <class, bool>
struct to_statically_typed_trait_oracle;

template <class T>
struct to_statically_typed_trait_oracle<T, true> {
  using type = statically_typed<T>;
};

template <class T>
struct to_statically_typed_trait_oracle<T, false> {
  using type = T;
};

/// Converts a template parameter pack of signatures to a trait type wrapping
/// the given signatures.
/// @note used for backwards compatibility when declaring typed interfaces.
template <class T>
using to_statically_typed_trait_t =
  typename to_statically_typed_trait_oracle<T, std::is_function_v<T>>::type;

} // namespace caf::detail
