// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/error.hpp"

namespace caf::net::dsl {

/// Meta programming utility for accessing the name of a type.
template <class T>
struct get_name {
  static constexpr std::string_view value = T::name;
};

template <>
struct get_name<error> {
  static constexpr std::string_view value = "fail";
};

} // namespace caf::net::dsl
