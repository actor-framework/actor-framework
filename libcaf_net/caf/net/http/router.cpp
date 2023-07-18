#include "caf/net/http/router.hpp"

#include "caf/net/http/request.hpp"
#include "caf/net/http/responder.hpp"
#include "caf/net/make_actor_shell.hpp"
#include "caf/net/multiplexer.hpp"

#include "caf/async/future.hpp"
#include "caf/disposable.hpp"

namespace caf::net::http {

// -- constructors and destructors ---------------------------------------------

router::~router() {
  for (auto& [id, hdl] : pending_)
    hdl.dispose();
}

// -- factories ----------------------------------------------------------------

std::unique_ptr<router> router::make(std::vector<route_ptr> routes) {
  return std::make_unique<router>(std::move(routes));
}

// -- properties ---------------------------------------------------------------

actor_shell* router::self() {
  if (shell_)
    return shell_.get();
  auto& mpx = down_->mpx();
  shell_ = make_actor_shell(mpx.system(), &mpx);
  return shell_.get();
}

// -- API for the responders ---------------------------------------------------

request router::lift(responder&& res) {
  auto prom = async::promise<response>();
  auto fut = prom.get_future();
  auto buf = std::vector<std::byte>{res.payload().begin(), res.payload().end()};
  auto impl = request::impl{res.header(), std::move(buf), std::move(prom)};
  auto lifted = request{std::make_shared<request::impl>(std::move(impl))};
  auto request_id = request_id_++;
  auto hdl = fut.bind_to(down_->mpx())
               .then(
                 [this, request_id](const response& res) {
                   down_->begin_header(res.code());
                   for (auto& [key, val] : res.header_fields())
                     down_->add_header_field(key, val);
                   std::ignore = down_->end_header();
                   down_->send_payload(res.body());
                   pending_.erase(request_id);
                 },
                 [this, request_id](const error& err) {
                   auto description = to_string(err);
                   down_->send_response(status::internal_server_error,
                                        "text/plain", description);
                   pending_.erase(request_id);
                 });
  pending_.emplace(request_id, std::move(hdl));
  return lifted;
}

void router::shutdown(const error& err) {
  abort(err);
  down_->shutdown(err);
}

// -- http::upper_layer implementation -----------------------------------------

void router::prepare_send() {
  // nop
}

bool router::done_sending() {
  return true;
}

void router::abort(const error&) {
  for (auto& [id, hdl] : pending_)
    hdl.dispose();
  pending_.clear();
}

error router::start(lower_layer* down) {
  down_ = down;
  down_->request_messages();
  return {};
}

ptrdiff_t router::consume(const request_header& hdr, const_byte_span payload) {
  for (auto& ptr : routes_)
    if (ptr->exec(hdr, payload, this))
      return static_cast<ptrdiff_t>(payload.size());
  down_->send_response(http::status::not_found, "text/plain", "Not found.");
  return static_cast<ptrdiff_t>(payload.size());
}

} // namespace caf::net::http
