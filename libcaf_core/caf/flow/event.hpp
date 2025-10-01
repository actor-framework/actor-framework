// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/error.hpp"

#include <variant>

namespace caf::flow {

struct on_complete_event {};

struct on_error_event {
  error what;
};

template <class T>
struct on_next_event {
  T item;
};

template <class T>
using event = std::variant<on_complete_event, on_error_event, on_next_event<T>>;

} // namespace caf::flow
