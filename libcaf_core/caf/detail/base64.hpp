// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/optional.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"

#include <string>

namespace caf::detail {

class CAF_CORE_EXPORT base64 {
public:
  static void encode(string_view str, std::string& out);

  static void encode(string_view str, byte_buffer& out);

  static void encode(const_byte_span bytes, std::string& out);

  static void encode(const_byte_span bytes, byte_buffer& out);

  static std::string encode(string_view str) {
    std::string result;
    encode(str, result);
    return result;
  }

  static std::string encode(const_byte_span bytes) {
    std::string result;
    encode(bytes, result);
    return result;
  }

  static bool decode(string_view in, std::string& out);

  static bool decode(string_view in, byte_buffer& out);

  static bool decode(const_byte_span bytes, std::string& out);

  static bool decode(const_byte_span bytes, byte_buffer& out);

  static optional<std::string> decode(string_view in) {
    std::string result;
    if (decode(in, result))
      return {std::move(result)};
    else
      return {};
  }

  static optional<std::string> decode(const_byte_span in) {
    std::string result;
    if (decode(in, result))
      return {std::move(result)};
    else
      return {};
  }
};

} // namespace caf::detail
