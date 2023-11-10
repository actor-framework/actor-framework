// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/async_client.hpp"

namespace caf::net::http {

async_client::~async_client() {
  // nop
}

// -- generic lower layer implementation -------------------------------------

void async_client::prepare_send() {
  // nop
}

bool async_client::done_sending() {
  return true;
}

void async_client::abort(const caf::error& reason) {
  CAF_LOG_ERROR("Response abortet with: " << to_string(reason));
  response_.set_error(reason);
}

// -- http::client lower layer implementation --------------------------------

caf::error async_client::start(http::lower_layer::client* ll) {
  down = ll;
  return send_request();
}

ptrdiff_t async_client::consume(const http::response_header& hdr,
                                caf::const_byte_span payload) {
  CAF_LOG_INFO("Received a message");
  http::response::fields_map fields;
  hdr.for_each_field([&fields](auto key, auto value) {
    fields.container().emplace_back(key, value);
  });
  http::response resp{static_cast<http::status>(hdr.status()),
                      std::move(fields),
                      byte_buffer{payload.begin(), payload.end()}};
  response_.set_value(std::move(resp));
  return static_cast<ptrdiff_t>(payload.size());
}

// -- utility functions --------------------------------------------------------

caf::error async_client::send_request() {
  down->begin_header(method_, path_);
  for (const auto& [key, value] : fields_)
    down->add_header_field(key, value);
  if (!payload_.empty())
    down->add_header_field("Content-Length", std::to_string(payload_.size()));
  down->end_header();
  if (!payload_.empty())
    down->send_payload(as_bytes(make_span(payload_)));
  // Await response.
  down->request_messages();
  return caf::none;
}

} // namespace caf::net::http
