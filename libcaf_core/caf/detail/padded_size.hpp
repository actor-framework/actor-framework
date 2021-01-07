// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>

namespace caf {
namespace detail {

/// Calculates the size for `T` including padding for aligning to `max_align_t`.
template <class T>
constexpr size_t padded_size_v
  = ((sizeof(T) / alignof(max_align_t))
     + static_cast<size_t>(sizeof(T) % alignof(max_align_t) != 0))
    * alignof(max_align_t);

} // namespace detail
} // namespace caf
