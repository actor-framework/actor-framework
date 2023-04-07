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

} // namespace caf::detail
