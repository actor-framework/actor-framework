// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/default_thread_count.hpp"

#include <algorithm>
#include <thread>

namespace caf::detail {

size_t default_thread_count() {
  return std::max(std::thread::hardware_concurrency(), 4u);
}

} // namespace caf::detail
