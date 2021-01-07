// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "caf/detail/core_export.hpp"
#include "caf/detail/make_meta_object.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/span.hpp"
#include "caf/type_id.hpp"

namespace caf::detail {

template <uint16_t First, uint16_t Second>
struct type_id_pair {};

template <class Range, uint16_t... Is>
struct type_id_sequence_helper;

template <uint16_t End, uint16_t... Is>
struct type_id_sequence_helper<type_id_pair<End, End>, Is...> {
  using type = std::integer_sequence<uint16_t, Is...>;
};

template <uint16_t Begin, uint16_t End, uint16_t... Is>
struct type_id_sequence_helper<type_id_pair<Begin, End>, Is...> {
  using type = typename type_id_sequence_helper<type_id_pair<Begin + 1, End>,
                                                Is..., Begin>::type;
};

template <class Range>
using make_type_id_sequence = typename type_id_sequence_helper<
  type_id_pair<Range::begin, Range::end>>::type;

} // namespace caf::detail

namespace caf {

/// @warning calling this after constructing any ::actor_system is unsafe and
///          causes undefined behavior.
template <class ProjectIds, uint16_t... Is>
void init_global_meta_objects_impl(std::integer_sequence<uint16_t, Is...>) {
  static_assert(sizeof...(Is) > 0);
  detail::meta_object src[] = {
    detail::make_meta_object<type_by_id_t<Is>>(type_name_by_id_v<Is>)...,
  };
  detail::set_global_meta_objects(ProjectIds::begin, make_span(src));
}

/// Initializes the global meta object table with all types in `ProjectIds`.
/// @warning calling this after constructing any ::actor_system is unsafe and
///          causes undefined behavior.
template <class ProjectIds>
void init_global_meta_objects() {
  detail::make_type_id_sequence<ProjectIds> seq;
  init_global_meta_objects_impl<ProjectIds>(seq);
}

} // namespace caf

namespace caf::core {

/// Initializes the meta objects of the core module.
CAF_CORE_EXPORT void init_global_meta_objects();

} // namespace caf::core
