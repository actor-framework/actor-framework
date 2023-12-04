// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/http/client.hpp"
#include "caf/net/http/response.hpp"

#include "caf/async/future.hpp"
#include "caf/async/promise.hpp"
#include "caf/disposable.hpp"

#include <chrono>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace caf::net::http {

/// HTTP client for sending requests and receiving responses via promises.
class CAF_NET_EXPORT async_client : public http::upper_layer::client {
public:
  static auto make(http::method method, std::string path,
                   unordered_flat_map<std::string, std::string> fields,
                   const_byte_span payload) {
    return std::make_unique<async_client>(method, std::move(path), fields,
                                          byte_buffer{payload.begin(),
                                                      payload.end()});
  }

  async_client(http::method method, std::string path,
               unordered_flat_map<std::string, std::string> fields,
               byte_buffer payload)
    : method_{method},
      path_{std::move(path)},
      fields_{std::move(fields)},
      payload_{std::move(payload)} {
  }

  virtual ~async_client();

  // -- generic lower layer implementation -------------------------------------
  void prepare_send() override;

  [[nodiscard]] bool done_sending() override;

  void abort(const caf::error& reason) override;

  // -- http::client lower layer implementation --------------------------------

  caf::error start(http::lower_layer::client* ll) override;

  ptrdiff_t consume(const http::response_header& hdr,
                    caf::const_byte_span payload) override;

  async::future<response> get_future() const {
    return response_.get_future();
  }

private:
  caf::error send_request();

  http::lower_layer::client* down = nullptr;
  http::method method_;
  std::string path_;
  unordered_flat_map<std::string, std::string> fields_;
  byte_buffer payload_;
  async::promise<response> response_;
};

} // namespace caf::net::http
