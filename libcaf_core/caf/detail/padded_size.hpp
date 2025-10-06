// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <cstddef>

namespace caf::detail {

/// Calculates the size for `T` including padding for aligning to `max_align_t`.
template <class T>
constexpr size_t padded_size_v = (sizeof(T) + alignof(std::max_align_t) - 1)
                                 / alignof(std::max_align_t)
                                 * alignof(max_align_t);

} // namespace caf::detail
