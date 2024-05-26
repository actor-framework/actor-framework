// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/type_traits.hpp"

#include <cstddef>
#include <string>
#include <type_traits>

namespace caf::detail {

enum class hex_format {
  uppercase,
  lowercase,
};

template <hex_format format = hex_format::uppercase, class Buf = std::string>
void append_hex(Buf& result, const void* vptr, size_t n) {
  using value_type = typename Buf::value_type;
  if (n == 0)
    return;
  auto xs = reinterpret_cast<const uint8_t*>(vptr);
  const char* tbl;
  if constexpr (format == hex_format::uppercase)
    tbl = "0123456789ABCDEF";
  else
    tbl = "0123456789abcdef";
  for (size_t i = 0; i < n; ++i) {
    auto c = xs[i];
    result.push_back(static_cast<value_type>(tbl[c >> 4]));
    result.push_back(static_cast<value_type>(tbl[c & 0x0F]));
  }
}

template <hex_format format = hex_format::uppercase, class T = int>
void append_hex(std::string& result, const T& x) {
  append_hex<format>(result, &x, sizeof(T));
}

} // namespace caf::detail
