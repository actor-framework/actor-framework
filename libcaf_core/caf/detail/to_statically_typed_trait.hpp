// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/type_list.hpp"
#include "caf/statically_typed.hpp"

#include <type_traits>

namespace caf::detail {

template <class Trait>
concept typed_actor_trait = detail::is_type_list_v<typename Trait::signatures>;

template <class...>
struct to_statically_typed_trait;

template <message_handler_signature... Signatures>
struct to_statically_typed_trait<Signatures...> {
  using type = statically_typed<Signatures...>;
};

template <typed_actor_trait Trait>
struct to_statically_typed_trait<Trait> {
  using type = Trait;
};

/// Converts a template parameter pack of signatures to a trait type wrapping
/// the given signatures.
/// @note used for backwards compatibility when declaring typed interfaces.
template <class... Ts>
using to_statically_typed_trait_t =
  typename to_statically_typed_trait<Ts...>::type;

} // namespace caf::detail
