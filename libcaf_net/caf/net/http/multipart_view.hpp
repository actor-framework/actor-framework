// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/header.hpp"
#include "caf/net/http/response_header.hpp"
#include "caf/net/http/status.hpp"

#include "caf/config_value.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/uri.hpp"

#include <string_view>
#include <vector>

namespace caf::net::http {

/// TODO
class CAF_NET_EXPORT multipart_view {
public:
  using fields_map = http::header::fields_map;

  struct part {
    fields_map fields;
    const_byte_span value;
  };

  using const_iterator = std::vector<part>::const_iterator;

  multipart_view(const response_header& header, const_byte_span body) noexcept {
    if (!header.is_multipart())
      return;
    parse_body();
  }

  std::string_view delimiter(const response_header& hdr) const noexcept {
    using namespace std::literals;
    auto ct = hdr.field("Content-Type");
    auto sv = "boundary="sv;
    auto it = std::search(ct.begin(), ct.end(), sv.begin(), sv.end());
    if (it == ct.end())
      return "";
    else
      return std::string_view(ct.data(), std::distance(ct.begin(), it));
  }

  const part& operator[](size_t id) const {
    return parts[id];
  }

  const part& at(size_t id) const {
    return parts.at(id);
  }

  size_t size() const {
    return parts.size();
  }

  size_t empty() const {
    return parts.empty();
  }

  /// Clears the header content and fields.
  void clear() noexcept;

  /*
  /// Default constructor.
  multipart_view() = default;
  /// Move constructor.
  multipart_view(multipart_view&&) = default;
  /// Move assignment operator.
  multipart_view& operator=(multipart_view&&) = default;
  /// Copy constructor.
  multipart_view(const multipart_view&);
  /// Copy assignment operator.
  multipart_view& operator=(const multipart_view&);
  */

  const_iterator begin() const {
    return parts.begin();
  }

  const_iterator end() const {
    return parts.end();
  }

private:
  // Reference to the header.
  http::response_header hdr;

  // Reference to the byte span
  caf::const_byte_span payload;

  // Parts
  std::vector<part> parts;

  void parse_body() noexcept {
  }
};

} // namespace caf::net::http
