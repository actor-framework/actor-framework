// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/byte_buffer.hpp"

namespace caf {

byte_buffer to_byte_buffer(const char* str, size_t len) {
  auto first = reinterpret_cast<const std::byte*>(str);
  return byte_buffer{first, first + len};
}

byte_buffer to_byte_buffer(const std::byte* data, size_t len) {
  return byte_buffer{data, data + len};
}

} // namespace caf
