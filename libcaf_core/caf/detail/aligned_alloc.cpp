// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/aligned_alloc.hpp"

#include "caf/config.hpp"

#include <cstdlib>

namespace caf::detail {

void* aligned_alloc(size_t alignment, size_t size) {
#ifdef CAF_WINDOWS
  return _aligned_malloc(size, alignment);
#else
  // Align the size to the next multiple of alignment.
  size = (size + alignment - 1) / alignment * alignment;
  return std::aligned_alloc(alignment, size);
#endif
}

void aligned_free(void* memblock) {
#ifdef CAF_WINDOWS
  _aligned_free(memblock);
#else
  std::free(memblock);
#endif
}

} // namespace caf::detail
