// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/stream.hpp"

#include "caf/detail/assert.hpp"

namespace caf {

ptrdiff_t stream::compare(const stream& other) const noexcept {
  if (source_ < other.source_)
    return -1;
  if (source_ == other.source_) {
    if (id_ < other.id_)
      return -1;
    if (id_ == other.id_) {
      CAF_ASSERT(type_ == other.type_);
      CAF_ASSERT(name_ == other.name_);
      return 0;
    }
  }
  return 1;
}

} // namespace caf
