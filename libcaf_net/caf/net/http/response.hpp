// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"

#include "caf/async/promise.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/span.hpp"
#include "caf/unordered_flat_map.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

namespace caf::net::http {

/// Handle type (implicitly shared) that represents an HTTP server response.
class CAF_NET_EXPORT response {
public:
  using fields_map = unordered_flat_map<std::string, std::string>;

  struct impl {
    status code;
    fields_map fields;
    byte_buffer body;
  };

  response(status code, fields_map fields, byte_buffer body);

  /// Returns the HTTP status code.
  status code() const {
    return pimpl_->code;
  }

  /// Returns the HTTP header fields.
  span<const std::pair<std::string, std::string>> header_fields() const {
    return pimpl_->fields.container();
  }

  /// Returns the HTTP body (payload).
  const_byte_span body() const {
    return make_span(pimpl_->body);
  }

private:
  std::shared_ptr<impl> pimpl_;
};

} // namespace caf::net::http
