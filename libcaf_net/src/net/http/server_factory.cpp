#include "caf/net/http/server_factory.hpp"

namespace caf::detail {

// TODO: there is currently no back-pressure from the worker to the server.

// -- http_request_producer ----------------------------------------------------

void http_request_producer::on_consumer_ready() {
  // nop
}

void http_request_producer::on_consumer_cancel() {
  // nop
}

void http_request_producer::on_consumer_demand(size_t) {
  // nop
}

void http_request_producer::ref_producer() const noexcept {
  ref();
}

void http_request_producer::deref_producer() const noexcept {
  deref();
}

bool http_request_producer::push(const net::http::request& item) {
  return buf_->push(item);
}

// -- http_flow_adapter --------------------------------------------------------

void http_flow_adapter::prepare_send() {
  // nop
}

bool http_flow_adapter::done_sending() {
  return true;
}

void http_flow_adapter::abort(const error&) {
  for (auto& pending : pending_)
    pending.dispose();
}

error http_flow_adapter::start(net::http::lower_layer* down) {
  down_ = down;
  down_->request_messages();
  return none;
}

ptrdiff_t http_flow_adapter::consume(const net::http::request_header& hdr,
                                     const_byte_span payload) {
  using namespace net::http;
  if (!pending_.empty()) {
    CAF_LOG_WARNING("received multiple requests from the same HTTP client: "
                    "not implemented yet (drop request)");
    return static_cast<ptrdiff_t>(payload.size());
  }
  auto prom = async::promise<response>();
  auto fut = prom.get_future();
  auto buf = std::vector<std::byte>{payload.begin(), payload.end()};
  auto impl = request::impl{hdr, std::move(buf), std::move(prom)};
  producer_->push(request{std::make_shared<request::impl>(std::move(impl))});
  auto hdl = fut.bind_to(*loop_).then(
    [this](const response& res) {
      down_->begin_header(res.code());
      for (auto& [key, val] : res.header_fields())
        down_->add_header_field(key, val);
      std::ignore = down_->end_header();
      down_->send_payload(res.body());
      down_->shutdown();
    },
    [this](const error& err) {
      auto description = to_string(err);
      down_->send_response(status::internal_server_error, "text/plain",
                           description);
      down_->shutdown();
    });
  pending_.emplace_back(std::move(hdl));
  return static_cast<ptrdiff_t>(payload.size());
}

} // namespace caf::detail
