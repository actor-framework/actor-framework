// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/config.hpp"

namespace caf::net::http {

intrusive_ptr<server_config>
server_config::make(dsl::server_config::lazy_t, const base_config& from,
                    uint16_t port, std::string bind_address) {
  auto res = make_counted<server_config>(from.mpx);
  if (auto* err = std::get_if<error>(&from.data)) {
    res->data = *err;
  } else {
    auto& src_data = std::get<dsl::generic_config::lazy>(from.data);
    res->data = dsl::server_config::lazy{src_data.ctx, port,
                                         std::move(bind_address)};
  }
  return res;
}

intrusive_ptr<server_config> server_config::make(dsl::server_config::socket_t,
                                                 const base_config& from,
                                                 tcp_accept_socket fd) {
  auto res = make_counted<server_config>(from.mpx);
  if (auto* err = std::get_if<error>(&from.data)) {
    res->data = *err;
    close(fd);
  } else {
    auto& src_data = std::get<dsl::generic_config::lazy>(from.data);
    res->data = dsl::server_config::socket{src_data.ctx, fd};
  }
  return res;
}

} // namespace caf::net::http
