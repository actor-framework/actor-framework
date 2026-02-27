// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/actor_shell.hpp"
#include "caf/net/http/arg_parser.hpp"
#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/request.hpp"
#include "caf/net/http/responder.hpp"
#include "caf/net/http/route.hpp"
#include "caf/net/http/upper_layer.hpp"

#include "caf/caf_deprecated.hpp"
#include "caf/detail/connection_guard.hpp"
#include "caf/expected.hpp"

#include <memory>

namespace caf::net::http {

/// Sits on top of a @ref server and dispatches incoming requests to
/// user-defined handlers.
class CAF_NET_EXPORT router : public upper_layer::server {
public:
  // -- constructors and destructors -------------------------------------------

  ~router() override;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<router> make(std::vector<route_ptr> routes);

  static std::unique_ptr<router>
  make(std::vector<route_ptr> routes,
       ::caf::detail::connection_guard_ptr guard);

  // -- properties -------------------------------------------------------------

  /// Returns a pointer to the underlying HTTP layer.
  virtual lower_layer::server* down() = 0;

  /// Returns an @ref actor_shell for this router that enables routes to
  /// interact with actors.
  virtual actor_shell* self() = 0;

  // -- API for the responders -------------------------------------------------

  /// Lifts a @ref responder to an @ref request object that allows asynchronous
  /// processing of the HTTP request.
  virtual request lift(responder&& res) = 0;

  CAF_DEPRECATED("use abort_and_shutdown instead")
  void shutdown(const error& err);

  virtual void abort_and_shutdown(const error& err) = 0;

protected:
  /// Creates a request from the given components. Only the router
  /// implementation may construct requests.
  static request make_request(request_header hdr, std::vector<std::byte> body,
                              async::promise<response> prom,
                              detail::connection_guard_ptr conn_guard);
};

} // namespace caf::net::http
