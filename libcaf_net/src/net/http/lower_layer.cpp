// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/lower_layer.hpp"

#include "caf/net/http/header_fields_map.hpp"
#include "caf/net/http/status.hpp"

#include <string>
#include <string_view>

using namespace std::literals;

namespace caf::net::http {

lower_layer::~lower_layer() {
  // nop
}

bool lower_layer::send_response(status code, std::string_view content_type,
                                const_byte_span content) {
  auto content_size = std::to_string(content.size());
  begin_header(code);
  add_header_field("Content-Type"sv, content_type);
  add_header_field("Content-Length"sv, content_size);
  return end_header() && send_payload(content);
}

bool lower_layer::send_response(status code, std::string_view content_type,
                                std::string_view content) {
  return send_response(code, content_type, as_bytes(make_span(content)));
}

} // namespace caf::net::http
