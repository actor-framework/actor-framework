// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <span>
#include <cstddef>

namespace caf {

// Use std::span directly
template <class T>
using span = std::span<T>;

// Forward declare byte span types
using byte_span = span<std::byte>;
using const_byte_span = span<const std::byte>;

// Helper functions for byte conversion
template <class T>
const_byte_span as_bytes(span<T> xs) {
  return {reinterpret_cast<const std::byte*>(xs.data()), xs.size_bytes()};
}

template <class T>
byte_span as_writable_bytes(span<T> xs) {
  return {reinterpret_cast<std::byte*>(xs.data()), xs.size_bytes()};
}

} // namespace caf
