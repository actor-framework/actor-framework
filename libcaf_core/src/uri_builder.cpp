/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/uri_builder.hpp"

#include "caf/detail/uri_impl.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/make_counted.hpp"

namespace caf {

uri_builder::uri_builder() : impl_(make_counted<detail::uri_impl>()) {
  // nop
}

uri_builder& uri_builder::scheme(std::string str) {
  impl_->scheme = std::move(str);
  return *this;
}

uri_builder& uri_builder::userinfo(std::string str) {
  impl_->authority.userinfo = std::move(str);
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
  impl_->path = std::move(str);
  return *this;
}

uri_builder& uri_builder::query(uri::query_map map) {
  impl_->query = std::move(map);
  return *this;
}

uri_builder& uri_builder::fragment(std::string str) {
  impl_->fragment = std::move(str);
  return *this;
}

uri uri_builder::make() {
  impl_->assemble_str();
  return uri{std::move(impl_)};
}

} // namespace caf
