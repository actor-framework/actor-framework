// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <span>

namespace caf {

template <class T>
using span [[deprecated("use std::span instead")]] = std::span<T>;

template <class T, size_t N>
[[deprecated("construct std::span directly instead")]]
auto make_span(T (&data)[N]) {
  return std::span{data, N};
}

template <class Container>
[[deprecated("construct std::span directly instead")]]
auto make_span(Container& container) {
  return std::span{container.data(), container.size()};
}

template <class T>
[[deprecated("construct std::span directly instead")]]
auto make_span(T* data, size_t size) {
  return std::span{data, size};
}

template <class T>
[[deprecated("construct std::span directly instead")]]
auto make_span(T* data, T* end) {
  return std::span{data, static_cast<size_t>(end - data)};
}

} // namespace caf
