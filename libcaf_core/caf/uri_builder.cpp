// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/uri_builder.hpp"

#include "caf/make_counted.hpp"

namespace caf {

uri_builder::uri_builder() : impl_(make_counted<uri::impl_type>()) {
  // nop
}

uri_builder& uri_builder::scheme(std::string str) {
  impl_->scheme = std::move(str);
  return *this;
}

uri_builder& uri_builder::userinfo(std::string str) {
  impl_->authority.userinfo = uri::userinfo_type{std::move(str), std::nullopt};
  return *this;
}

uri_builder& uri_builder::userinfo(std::string str, std::string password) {
  impl_->authority.userinfo = uri::userinfo_type{std::move(str),
                                                 std::move(password)};
  return *this;
}

uri_builder& uri_builder::host(std::string str) {
  // IPv6 addresses get special attention from URIs, i.e., they go in between
  // square brackets. However, the parser does not catch IPv4 addresses. Hence,
  // we do a quick check here whether `str` contains a valid IPv4 address and
  // store it as IP address if possible.
  ipv4_address addr;
  if (auto err = parse(str, addr))
    impl_->authority.host = std::move(str);
  else
    impl_->authority.host = ip_address{addr};
  return *this;
}

uri_builder& uri_builder::host(ip_address addr) {
  impl_->authority.host = addr;
  return *this;
}

uri_builder& uri_builder::port(uint16_t value) {
  impl_->authority.port = value;
  return *this;
}

uri_builder& uri_builder::path(std::string str) {
  uri::decode(str);
  impl_->path = std::move(str);
  return *this;
}

uri_builder& uri_builder::query(uri::query_map map) {
  for (auto [key, val] : map) {
    uri::decode(key);
    uri::decode(val);
    impl_->query.emplace(std::move(key), std::move(val));
  }
  return *this;
}

uri_builder& uri_builder::fragment(std::string str) {
  uri::decode(str);
  impl_->fragment = std::move(str);
  return *this;
}

uri uri_builder::make() {
  impl_->assemble_str();
  return uri{std::move(impl_)};
}

} // namespace caf
