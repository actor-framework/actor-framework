// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/prometheus/server.hpp"

#include "caf/net/http/header.hpp"
#include "caf/net/http/lower_layer.hpp"

using namespace std::literals;

namespace caf::net::prometheus {

// -- server::scrape_state -----------------------------------------------------

std::string_view server::scrape_state::scrape() {
  // Scrape system metrics at most once per second. TODO: make configurable.
  if (auto now = std::chrono::steady_clock::now(); last_scrape + 1s <= now) {
    last_scrape = now;
    proc_importer.update();
  }
  return collector.collect_from(*registry);
}

// -- implementation of http::upper_layer --------------------------------------

void server::prepare_send() {
  // nop
}

bool server::done_sending() {
  return true;
}

void server::abort(const error&) {
  // nop
}

error server::start(http::lower_layer* down, const settings&) {
  down_ = down;
  down_->request_messages();
  return caf::none;
}

ptrdiff_t server::consume(const http::header& hdr, const_byte_span payload) {
  if (hdr.path() != "/metrics") {
    down_->send_response(http::status::not_found, "text/plain", "Not found.");
    down_->shutdown();
  } else if (hdr.method() != http::method::get) {
    down_->send_response(http::status::method_not_allowed, "text/plain",
                         "Method not allowed.");
    down_->shutdown();
  } else if (!hdr.query().empty() || !hdr.fragment().empty()) {
    down_->send_response(http::status::bad_request, "text/plain",
                         "No fragment or query allowed.");
    down_->shutdown();
  } else {
    auto str = state_->scrape();
    down_->send_response(http::status::ok, "text/plain;version=0.0.4", str);
    down_->shutdown();
  }
  return static_cast<ptrdiff_t>(payload.size());
}

} // namespace caf::net::prometheus
