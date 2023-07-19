// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <atomic>

namespace caf::detail {

template <class T>
bool cas_weak(std::atomic<T>* obj, T* expected, T desired) {
#if (defined(CAF_CLANG) && CAF_COMPILER_VERSION < 30401)                       \
  || (defined(CAF_GCC) && CAF_COMPILER_VERSION < 40803)
  return std::atomic_compare_exchange_strong(obj, expected, desired);
#else
  return std::atomic_compare_exchange_weak(obj, expected, desired);
#endif
}

} // namespace caf::detail
