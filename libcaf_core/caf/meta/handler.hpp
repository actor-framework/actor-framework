// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/type_id_list.hpp"
#include "caf/type_list.hpp"

#include <string>

namespace caf::meta {

/// Descriptor for a message handler handler.
struct handler {
  type_id_list inputs;
  type_id_list outputs;
};

/// @relates handler
CAF_CORE_EXPORT std::string to_string(handler hdl);

template <class... Out>
struct result_to_type_id_list {
  static constexpr auto value = make_type_id_list<Out...>();
};

template <>
struct result_to_type_id_list<void> {
  static constexpr auto value = make_type_id_list<>();
};

template <>
struct result_to_type_id_list<unit_t> {
  static constexpr auto value = make_type_id_list<>();
};

template <class T>
struct handler_from_signature;

template <class... Out, class... In>
struct handler_from_signature<result<Out...>(In...)> {
  static constexpr handler value = {make_type_id_list<In...>(),
                                    result_to_type_id_list<Out...>::value};
};

template <class List>
struct handlers_from_signature_list;

template <class... Signature>
struct handlers_from_signature_list<type_list<Signature...>> {
  static inline constexpr meta::handler handlers[]
    = {handler_from_signature<Signature>::value...,
       handler{type_id_list{nullptr}, type_id_list{nullptr}}};
};

template <>
struct handlers_from_signature_list<none_t> {
  static inline constexpr meta::handler* handlers = nullptr;
};

} // namespace caf::meta
