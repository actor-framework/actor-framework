// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config_value.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/http/header_fields_map.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/status.hpp"
#include "caf/string_view.hpp"
#include "caf/uri.hpp"

#include <vector>

namespace caf::net::http {

class CAF_NET_EXPORT header {
public:
  header() = default;

  header(header&&) = default;

  header& operator=(header&&) = default;

  header(const header&);

  header& operator=(const header&);

  void assign(const header&);

  http::method method() const noexcept {
    return method_;
  }

  string_view path() const noexcept {
    return uri_.path();
  }

  const uri::query_map& query() const noexcept {
    return uri_.query();
  }

  string_view fragment() const noexcept {
    return uri_.fragment();
  }

  string_view version() const noexcept {
    return version_;
  }

  const header_fields_map& fields() const noexcept {
    return fields_;
  }

  string_view field(string_view key) const noexcept {
    if (auto i = fields_.find(key); i != fields_.end())
      return i->second;
    else
      return {};
  }

  template <class T>
  optional<T> field_as(string_view key) const noexcept {
    if (auto i = fields_.find(key); i != fields_.end()) {
      caf::config_value val{to_string(i->second)};
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

  std::pair<status, string_view> parse(string_view raw);

  bool chunked_transfer_encoding() const;

  optional<size_t> content_length() const;

private:
  std::vector<char> raw_;
  http::method method_;
  uri uri_;
  string_view version_;
  header_fields_map fields_;
};

} // namespace caf::net::http
