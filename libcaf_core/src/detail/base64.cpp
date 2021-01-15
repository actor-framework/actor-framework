// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/base64.hpp"

#include <cstdint>

namespace caf::detail {

namespace {

// clang-format off
constexpr uint8_t decoding_tbl[] = {
/*       ..0 ..1 ..2 ..3 ..4 ..5 ..6 ..7 ..8 ..9 ..A ..B ..C ..D ..E ..F  */
/* 0.. */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 1.. */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 2.. */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62,  0,  0,  0, 63,
/* 3.. */ 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,
/* 4.. */  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
/* 5.. */ 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,  0,  0,  0,  0,
/* 6.. */  0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
/* 7.. */ 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,  0,  0,  0,  0,  0};
// clang-format on

constexpr const char encoding_tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "abcdefghijklmnopqrstuvwxyz"
                                      "0123456789+/";

template <class Storage>
void encode_impl(string_view in, Storage& out) {
  using value_type = typename Storage::value_type;
  // Consumes three characters from the input at once.
  auto consume = [&out](const char* str) {
    int buf[] = {
      (str[0] & 0xfc) >> 2,
      ((str[0] & 0x03) << 4) + ((str[1] & 0xf0) >> 4),
      ((str[1] & 0x0f) << 2) + ((str[2] & 0xc0) >> 6),
      str[2] & 0x3f,
    };
    for (auto x : buf)
      out.push_back(static_cast<value_type>(encoding_tbl[x]));
  };
  // Iterate the input in chunks of three bytes.
  auto i = in.begin();
  for (; std::distance(i, in.end()) >= 3; i += 3)
    consume(i);
  // Deal with any leftover: pad the input with zeros and then fixup the output.
  if (i != in.end()) {
    char buf[] = {0, 0, 0};
    std::copy(i, in.end(), buf);
    consume(buf);
    for (auto j = out.end() - (3 - (in.size() % 3)); j != out.end(); ++j)
      *j = static_cast<value_type>('=');
  }
}

template <class Storage>
bool decode_impl(string_view in, Storage& out) {
  using value_type = typename Storage::value_type;
  // Short-circuit empty inputs.
  if (in.empty())
    return true;
  // Refuse invalid inputs: Base64 always produces character groups of size 4.
  if (in.size() % 4 != 0)
    return false;
  // Consume four characters from the input at once.
  auto val = [](char c) { return decoding_tbl[c & 0x7F]; };
  for (auto i = in.begin(); i != in.end(); i += 4) {
    // clang-format off
    auto bits =   (val(i[0]) << 18)
                | (val(i[1]) << 12)
                | (val(i[2]) << 6)
                | (val(i[3]));
    // clang-format on
    out.push_back(static_cast<value_type>((bits & 0xFF0000) >> 16));
    out.push_back(static_cast<value_type>((bits & 0x00FF00) >> 8));
    out.push_back(static_cast<value_type>((bits & 0x0000FF)));
  }
  // Fix up the output buffer if the input contained padding.
  auto s = in.size();
  if (in[s - 2] == '=') {
    out.pop_back();
    out.pop_back();
  } else if (in[s - 1] == '=') {
    out.pop_back();
  }
  return true;
}

string_view as_string_view(const_byte_span bytes) {
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

} // namespace

void base64::encode(string_view str, std::string& out) {
  encode_impl(str, out);
}

void base64::encode(string_view str, byte_buffer& out) {
  encode_impl(str, out);
}

void base64::encode(const_byte_span bytes, std::string& out) {
  encode_impl(as_string_view(bytes), out);
}

void base64::encode(const_byte_span bytes, byte_buffer& out) {
  encode_impl(as_string_view(bytes), out);
}

bool base64::decode(string_view in, std::string& out) {
  return decode_impl(in, out);
}

bool base64::decode(string_view in, byte_buffer& out) {
  return decode_impl(in, out);
}

bool base64::decode(const_byte_span bytes, std::string& out) {
  return decode_impl(as_string_view(bytes), out);
}

bool base64::decode(const_byte_span bytes, byte_buffer& out) {
  return decode_impl(as_string_view(bytes), out);
}

} // namespace caf::detail
