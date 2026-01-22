#include "caf/net/http/router.hpp"

#include "caf/net/http/request.hpp"
#include "caf/net/http/responder.hpp"
#include "caf/net/make_actor_shell.hpp"
#include "caf/net/multiplexer.hpp"

#include "caf/async/future.hpp"
#include "caf/disposable.hpp"
#include "caf/log/net.hpp"
#include "caf/make_counted.hpp"

#include <atomic>

namespace caf::net::http {

namespace {

/// Trivial connection guard that only tracks the orphaned flag.
class trivial_connection_guard : public detail::connection_guard {
public:
  bool orphaned() const noexcept override {
    return orphaned_.load(std::memory_order_acquire);
  }

  void set_orphaned() noexcept override {
    orphaned_.store(true, std::memory_order_release);
  }

private:
  std::atomic<bool> orphaned_{false};
};

} // namespace

// -- constructors and destructors ---------------------------------------------

router::router() : guard_(make_counted<trivial_connection_guard>()) {
  // nop
}

router::router(std::vector<route_ptr> routes)
  : routes_(std::move(routes)),
    guard_(make_counted<trivial_connection_guard>()) {
  // nop
}

router::router(std::vector<route_ptr> routes,
               detail::connection_guard_ptr guard)
  : routes_(std::move(routes)), guard_(std::move(guard)) {
  CAF_ASSERT(guard_ != nullptr);
}

router::~router() {
  guard_->set_orphaned();
  for (auto& [id, hdl] : pending_)
    hdl.dispose();
}

// -- factories ----------------------------------------------------------------

std::unique_ptr<router> router::make(std::vector<route_ptr> routes) {
  return std::make_unique<router>(std::move(routes));
}

std::unique_ptr<router> router::make(std::vector<route_ptr> routes,
                                     detail::connection_guard_ptr guard) {
  return std::make_unique<router>(std::move(routes), std::move(guard));
}

// -- properties ---------------------------------------------------------------

actor_shell* router::self() {
  if (shell_)
    return shell_.get();
  shell_ = make_actor_shell(down_->manager());
  return shell_.get();
}

// -- API for the responders ---------------------------------------------------

request router::lift(responder&& res) {
  CAF_ASSERT(guard_ != nullptr);
  auto prom = async::promise<response>();
  auto fut = prom.get_future();
  auto buf = std::vector<std::byte>{res.payload().begin(), res.payload().end()};
  auto lifted = request{res.header(), std::move(buf), std::move(prom), guard_};
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
  abort_and_shutdown(err);
}

void router::abort_and_shutdown(const error& err) {
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

void router::abort(const error& reason) {
  log::net::debug("HTTP router aborted with reason: {}", reason);
  for (auto& [id, hdl] : pending_)
    hdl.dispose();
  pending_.clear();
}

error router::start(lower_layer::server* down) {
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

error router::begin_chunked_message(const net::http::request_header& hdr) {
  hdr_ = hdr;
  return error{};
}

error router::consume_chunk(const_byte_span body) {
  CAF_ASSERT(hdr_.valid());
  body_.insert(body_.end(), body.begin(), body.end());
  return error{};
}

error router::end_chunked_message() {
  auto ret = consume(hdr_, body_);
  body_.clear();
  hdr_ = request_header{};
  if (ret < 0)
    return error{sec::protocol_error,
                 "Failed to process the end of the chunked request."};
  return error{};
}

} // namespace caf::net::http
