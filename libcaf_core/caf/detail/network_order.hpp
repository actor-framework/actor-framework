/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/config.hpp"

namespace caf {
namespace detail {

#ifdef CAF_MSVC // we assume Windows is always little endian

inline uint16_t to_network_order(uint16_t value) {
  return _byteswap_ushort(value);
}

inline uint32_t to_network_order(uint32_t value) {
  return _byteswap_ulong(value);
}

inline uint64_t to_network_order(uint64_t value) {
  return _byteswap_uint64(value);
}

#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

inline uint16_t to_network_order(uint16_t value) {
  return __builtin_bswap16(value);
}

inline uint32_t to_network_order(uint32_t value) {
  return __builtin_bswap32(value);
}

inline uint64_t to_network_order(uint64_t value) {
  return __builtin_bswap64(value);
}

#else

template <class T>
inline T to_network_order(T value) {
  return value;
}

#endif

template <class T>
inline T from_network_order(T value) {
  // swapping the bytes again gives the native order
  return to_network_order(value);
}

} // namespace detail
} // namespace caf

