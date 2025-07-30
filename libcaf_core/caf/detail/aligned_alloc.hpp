// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <cstddef>

namespace caf::detail {

/// Allocates memory with a specified alignment.
/// @param alignment Must be a power of two.
/// @param size The number of bytes to allocate.
/// @return A pointer to the allocated memory on success, `nullptr` on failure.
CAF_CORE_EXPORT void* aligned_alloc(size_t alignment, size_t size);

/// Frees memory allocated with `aligned_alloc`.
/// @param memblock The pointer allocated with `aligned_alloc`.
CAF_CORE_EXPORT void aligned_free(void* memblock);

} // namespace caf::detail
