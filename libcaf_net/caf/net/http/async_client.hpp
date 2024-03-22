// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/http/response.hpp"
#include "caf/net/http/upper_layer.hpp"

#include "caf/async/future.hpp"

namespace caf::net::http {

/// HTTP client for sending requests and receiving responses via promises.
class CAF_NET_EXPORT async_client : public http::upper_layer::client {
public:
  // -- factories --------------------------------------------------------------

  static std::unique_ptr<async_client>
  make(http::method method, std::string path,
       unordered_flat_map<std::string, std::string> fields,
       const_byte_span payload);

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~async_client();

  // -- properties -------------------------------------------------------------

  virtual async::future<response> get_future() const = 0;
};

} // namespace caf::net::http
