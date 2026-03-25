// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <concepts>

namespace caf::detail {

enum class memory_interface {
  new_and_delete,
  malloc_and_free,
};

template <class T>
concept has_memory_interface = requires(T) {
  { T::memory_interface } -> std::same_as<memory_interface>;
};

} // namespace caf::detail
