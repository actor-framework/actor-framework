/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/fnv_hash.hpp"

#include <cstdint>

namespace caf {
namespace detail {

namespace {

#if SIZE_MAX == 0xFFFFFFFF

constexpr size_t basis = 2166136261u;

constexpr size_t prime = 16777619u;

#elif SIZE_MAX == 0xFFFFFFFFFFFFFFFF

constexpr size_t basis = 14695981039346656037u;

constexpr size_t prime = 1099511628211u;

#else

#  error Platform and/or compiler not supported

#endif

} // namespace

size_t fnv_hash(const unsigned char* first, const unsigned char* last) {
  return fnv_hash_append(basis, first, last);
}

size_t fnv_hash_append(size_t intermediate, const unsigned char* first,
                       const unsigned char* last) {
  auto result = intermediate;
  for (; first != last; ++first) {
    result *= prime;
    result ^= *first;
  }
  return result;
}

} // namespace detail
} // namespace caf
