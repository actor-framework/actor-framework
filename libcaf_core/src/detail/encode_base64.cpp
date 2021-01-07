// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/encode_base64.hpp"

namespace caf::detail {

namespace {

constexpr const char base64_tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz"
                                    "0123456789+/";

} // namespace

std::string encode_base64(string_view str) {
  auto bytes = make_span(reinterpret_cast<const byte*>(str.data()), str.size());
  return encode_base64(bytes);
}

std::string encode_base64(span<const byte> bytes) {
  std::string result;
  // Consumes three characters from input at once.
  auto consume = [&result](const byte* i) {
    auto at = [i](size_t index) { return to_integer<int>(i[index]); };
    int buf[] = {
      (at(0) & 0xfc) >> 2,
      ((at(0) & 0x03) << 4) + ((at(1) & 0xf0) >> 4),
      ((at(1) & 0x0f) << 2) + ((at(2) & 0xc0) >> 6),
      at(2) & 0x3f,
    };
    for (auto x : buf)
      result += base64_tbl[x];
  };
  // Iterate the input in chunks of three bytes.
  auto i = bytes.begin();
  for (; std::distance(i, bytes.end()) >= 3; i += 3)
    consume(i);
  if (i != bytes.end()) {
    // Pad input with zeros.
    byte buf[] = {byte{0}, byte{0}, byte{0}};
    std::copy(i, bytes.end(), buf);
    consume(buf);
    // Override padded bytes (garbage) with '='.
    for (auto j = result.end() - (3 - (bytes.size() % 3)); j != result.end();
         ++j)
      *j = '=';
  }
  return result;
}

} // namespace caf::detail
