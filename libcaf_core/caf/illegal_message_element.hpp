// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <type_traits>

namespace caf {

/// Marker class identifying classes in CAF that are not allowed
/// to be used as message element.
struct illegal_message_element {
  // no members (marker class)
};

template <class T>
struct is_illegal_message_element
  : std::is_base_of<illegal_message_element, T> {};

} // namespace caf
