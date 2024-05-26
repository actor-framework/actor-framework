// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/actor_shell.hpp"
#include "caf/net/http/arg_parser.hpp"
#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/responder.hpp"
#include "caf/net/http/route.hpp"
#include "caf/net/http/upper_layer.hpp"

#include "caf/detail/print.hpp"
#include "caf/expected.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"

#include <algorithm>
#include <cassert>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace caf::net::http {

/// Sits on top of a @ref server and dispatches incoming requests to
/// user-defined handlers.
class CAF_NET_EXPORT router : public upper_layer::server {
public:
  // -- constructors and destructors -------------------------------------------

  router() = default;

  explicit router(std::vector<route_ptr> routes) : routes_(std::move(routes)) {
    // nop
  }

  ~router() override;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<router> make(std::vector<route_ptr> routes);

  // -- properties -------------------------------------------------------------

  /// Returns a pointer to the underlying HTTP layer.
  lower_layer::server* down() {
    return down_;
  }

  /// Returns an @ref actor_shell for this router that enables routes to
  /// interact with actors.
  actor_shell* self();

  // -- API for the responders -------------------------------------------------

  /// Lifts a @ref responder to an @ref request object that allows asynchronous
  /// processing of the HTTP request.
  request lift(responder&& res);

  void shutdown(const error& err);

  // -- http::upper_layer implementation ---------------------------------------

  error start(lower_layer::server* down) override;

  ptrdiff_t consume(const request_header& hdr,
                    const_byte_span payload) override;

  void prepare_send() override;

  bool done_sending() override;

  void abort(const error& reason) override;

private:
  /// Handle to the underlying HTTP layer.
  lower_layer::server* down_ = nullptr;

  /// List of user-defined routes.
  std::vector<route_ptr> routes_;

  /// Generates ascending IDs for `pending_`.
  size_t request_id_ = 0;

  /// Keeps track of pending HTTP requests when lifting @ref responder objects.
  std::unordered_map<size_t, disposable> pending_;

  /// Lazily initialized for allowing a @ref route to interact with actors.
  actor_shell_ptr shell_;
};

} // namespace caf::net::http
