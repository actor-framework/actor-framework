// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config_value.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/http/header_fields_map.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/status.hpp"
#include "caf/uri.hpp"

#include <string_view>
#include <vector>

namespace caf::net::http {

/// Encapsulates meta data for HTTP requests.
class CAF_NET_EXPORT request_header {
public:
  request_header() = default;

  request_header(request_header&&) = default;

  request_header& operator=(request_header&&) = default;

  request_header(const request_header&);

  request_header& operator=(const request_header&);

  void assign(const request_header&);

  http::method method() const noexcept {
    return method_;
  }

  std::string_view path() const noexcept {
    return uri_.path();
  }

  const uri::query_map& query() const noexcept {
    return uri_.query();
  }

  std::string_view fragment() const noexcept {
    return uri_.fragment();
  }

  std::string_view version() const noexcept {
    return version_;
  }

  const header_fields_map& fields() const noexcept {
    return fields_;
  }

  std::string_view field(std::string_view key) const noexcept {
    if (auto i = fields_.find(key); i != fields_.end())
      return i->second;
    else
      return {};
  }

  template <class T>
  std::optional<T> field_as(std::string_view key) const noexcept {
    if (auto i = fields_.find(key); i != fields_.end()) {
      caf::config_value val{std::string{i->second}};
      if (auto res = caf::get_as<T>(val))
        return std::move(*res);
      else
        return {};
    } else {
      return {};
    }
  }

  bool valid() const noexcept {
    return !raw_.empty();
  }

  std::pair<status, std::string_view> parse(std::string_view raw);

  bool chunked_transfer_encoding() const;

  std::optional<size_t> content_length() const;

private:
  std::vector<char> raw_;
  http::method method_;
  uri uri_;
  std::string_view version_;
  header_fields_map fields_;
};

} // namespace caf::net::http
