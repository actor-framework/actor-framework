#include "caf/net/http/responder.hpp"

#include "caf/net/http/request.hpp"
#include "caf/net/http/router.hpp"

namespace caf::net::http {

responder::promise_state::~promise_state() {
  if (!completed_) {
    down_->send_response(status::internal_server_error, "text/plain",
                         "Internal server error: broken responder promise.");
  }
}

responder::promise::promise(responder& parent)
  : impl_(make_counted<promise_state>(parent.down())) {
  // nop
}

actor_shell* responder::self() {
  return router_->self();
}

lower_layer::server* responder::down() {
  return router_->down();
}

request responder::to_request() && {
  return router_->lift(std::move(*this));
}

responder::promise responder::to_promise() && {
  return promise{*this};
}

} // namespace caf::net::http
