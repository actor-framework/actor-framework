// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/async_client.hpp"

#include "caf/net/http/response.hpp"
#include "caf/net/http/response_header.hpp"

#include "caf/async/future.hpp"
#include "caf/async/promise.hpp"
#include "caf/log/net.hpp"

#include <utility>

namespace caf::net::http {

namespace {

/// HTTP client for sending requests and receiving responses via promises.
class async_client_impl : public async_client {
public:
  // -- constructors, destructors, and assignment operators --------------------
  async_client_impl(http::method method, std::string path,
                    unordered_flat_map<std::string, std::string> fields,
                    byte_buffer payload)
    : method_{method},
      path_{std::move(path)},
      fields_{std::move(fields)},
      payload_{std::move(payload)} {
  }

  // -- generic lower layer implementation -------------------------------------

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  void abort(const caf::error& reason) override {
    log::net::error("Response abortet with: {}", to_string(reason));
    response_.set_error(reason);
  }

  // -- http::client lower layer implementation --------------------------------

  caf::error start(http::lower_layer::client* ll) override {
    down = ll;
    return send_request();
  }

  ptrdiff_t consume(const http::response_header& hdr,
                    caf::const_byte_span payload) override {
    log::net::info("Received a message");
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

  // -- http::async_client implementation --------------------------------------

  async::future<response> get_future() const override {
    return response_.get_future();
  }

private:
  // -- utility functions ------------------------------------------------------

  caf::error send_request() {
    down->begin_header(method_, path_);
    for (const auto& [key, value] : fields_)
      down->add_header_field(key, value);
    if (!payload_.empty())
      down->add_header_field("Content-Length", std::to_string(payload_.size()));
    down->end_header();
    if (!payload_.empty())
      down->send_payload(payload_);
    // Await response.
    down->request_messages();
    return caf::none;
  }

  http::lower_layer::client* down = nullptr;
  http::method method_;
  std::string path_;
  unordered_flat_map<std::string, std::string> fields_;
  byte_buffer payload_;
  async::promise<response> response_;
};

} // namespace

// -- factories ----------------------------------------------------------------

std::unique_ptr<async_client>
async_client::make(http::method method, std::string path,
                   unordered_flat_map<std::string, std::string> fields,
                   const_byte_span payload) {
  return std::make_unique<async_client_impl>(method, std::move(path), fields,
                                             byte_buffer{payload.begin(),
                                                         payload.end()});
}

// -- constructors, destructors, and assignment operators ----------------------

async_client::~async_client() {
  // nop
}

} // namespace caf::net::http
