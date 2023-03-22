#include "caf/net/http/responder.hpp"

#include "caf/net/http/request.hpp"
#include "caf/net/http/router.hpp"

namespace caf::net::http {

lower_layer* responder::down() {
  return router_->down();
}

request responder::to_request() && {
  return router_->lift(std::move(*this));
}

} // namespace caf::net::http
