// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/request.hpp"

using namespace std::literals;

// TODO: reduce number of memory allocations for producing the response.

namespace caf::net::http {

void request::respond(status code, std::string_view content_type,
                      const_byte_span content) const {
  auto content_size = std::to_string(content.size());
  unordered_flat_map<std::string, std::string> fields;
  fields.emplace("Content-Type"sv, content_type);
  fields.emplace("Content-Length"sv, content_size);
  auto body = std::vector<std::byte>{content.begin(), content.end()};
  pimpl_->prom.set_value(response{code, std::move(fields), std::move(body)});
}

} // namespace caf::net::http
