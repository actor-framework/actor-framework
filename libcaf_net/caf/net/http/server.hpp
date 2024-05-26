// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/upper_layer.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"

#include "caf/detail/net_export.hpp"

namespace caf::net::http {

/// Implements the server part for the HTTP Protocol as defined in RFC 7231.
class CAF_NET_EXPORT server : public octet_stream::upper_layer,
                              public http::lower_layer::server {
public:
  // -- member types -----------------------------------------------------------

  using upper_layer_ptr = std::unique_ptr<http::upper_layer::server>;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<server> make(upper_layer_ptr up);

  // -- constructors, destructors, and assignment operators --------------------

  ~server() override;

  // -- properties -------------------------------------------------------------

  virtual size_t max_request_size() const noexcept = 0;

  virtual void max_request_size(size_t value) noexcept = 0;
};

} // namespace caf::net::http
