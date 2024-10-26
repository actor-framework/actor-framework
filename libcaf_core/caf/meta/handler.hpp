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
inline bool operator==(const handler& lhs, const handler& rhs) noexcept {
  return lhs.inputs == rhs.inputs && lhs.outputs == rhs.outputs;
}

/// @relates handler
inline bool operator!=(const handler& lhs, const handler& rhs) noexcept {
  return !(lhs == rhs);
}

/// @relates handler
CAF_CORE_EXPORT std::string to_string(const handler& hdl);

/// Represents a list of message handlers for describing the interface of a
/// statically typed actor.
struct CAF_CORE_EXPORT handler_list {
  /// The number of handlers in this list.
  size_t size;

  /// A pointer to the first handler in this list.
  const handler* data;

  /// Checks whether this list is empty.
  bool empty() const noexcept {
    return size == 0;
  }

  /// Returns an iterator to the first handler in this list.
  auto begin() const noexcept {
    return data;
  }

  /// Returns the past-the-end iterator of this list.
  auto end() const noexcept {
    return data + size;
  }

  /// Checks whether this list contains the given handler.
  bool contains(const handler& what) const noexcept;
};

/// @relates handler_list
CAF_CORE_EXPORT std::string to_string(const handler_list& list);

/// Checks whether @p rhs can be assigned to @p lhs, i.e., whether each handler
/// in @p lhs is present in @p rhs.
/// @relates handler_list
/// @param lhs The left-hand side list for the assignment.
/// @param rhs The right-hand side list for the assignment.
CAF_CORE_EXPORT bool assignable(const handler_list& lhs,
                                const handler_list& rhs) noexcept;

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

template <>
struct handlers_from_signature_list<type_list<>> {
  static constexpr meta::handler_list handlers{0, nullptr};
};

template <class... Signature>
struct handlers_from_signature_list<type_list<Signature...>> {
  static constexpr meta::handler handlers_data[]
    = {handler_from_signature<Signature>::value...};
  static constexpr meta::handler_list handlers{sizeof...(Signature),
                                               handlers_data};
};

} // namespace caf::meta
