// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/upper_layer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"

#include <memory>

namespace caf::net::http {

/// Implements the client part for the HTTP Protocol as defined in RFC 7231.
class CAF_NET_EXPORT client : public octet_stream::upper_layer,
                              public http::lower_layer::client {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<http::upper_layer::client>;

  // -- constructors, destructors, and assignment operators --------------------

  ~client() override;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<client> make(upper_layer_ptr up);

  // -- properties -------------------------------------------------------------

  virtual size_t max_response_size() const = 0;

  virtual void max_response_size(size_t value) = 0;
};

} // namespace caf::net::http
