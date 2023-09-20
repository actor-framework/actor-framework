// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/prometheus_broker.hpp"

#include "caf/span.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/telemetry/dbl_gauge.hpp"
#include "caf/telemetry/int_gauge.hpp"

#include <string_view>

namespace caf::detail {

namespace {

// Cap incoming HTTP requests.
constexpr size_t max_request_size = 512 * 1024ul;

// HTTP response for requests that exceed the size limit.
constexpr std::string_view request_too_large
  = "HTTP/1.1 413 Request Entity Too Large\r\n"
    "Connection: Closed\r\n\r\n";

// HTTP response for requests that aren't "GET /metrics HTTP/1.1".
constexpr std::string_view request_not_supported
  = "HTTP/1.1 501 Not Implemented\r\n"
    "Connection: Closed\r\n\r\n";

// HTTP header when sending a payload.
constexpr std::string_view request_ok = "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: text/plain\r\n"
                                        "Connection: Closed\r\n\r\n";

} // namespace

prometheus_broker::prometheus_broker(actor_config& cfg)
  : io::broker(cfg), proc_importer_(system().metrics()) {
  // nop
}

prometheus_broker::prometheus_broker(actor_config& cfg, io::doorman_ptr ptr)
  : prometheus_broker(cfg) {
  add_doorman(std::move(ptr));
}

prometheus_broker::~prometheus_broker() {
  // nop
}

const char* prometheus_broker::name() const {
  return "caf.system.prometheus-broker";
}

behavior prometheus_broker::make_behavior() {
  return {
    [this](const io::new_data_msg& msg) {
      auto flush_and_close = [this, &msg] {
        flush(msg.handle);
        close(msg.handle);
        requests_.erase(msg.handle);
        if (num_connections() + num_doormen() == 0)
          quit();
      };
      auto& req = requests_[msg.handle];
      if (req.size() + msg.buf.size() > max_request_size) {
        write(msg.handle, as_bytes(make_span(request_too_large)));
        flush_and_close();
        return;
      }
      req.insert(req.end(), msg.buf.begin(), msg.buf.end());
      auto req_str = std::string_view{reinterpret_cast<char*>(req.data()),
                                      req.size()};
      // Stop here if the header isn't complete yet.
      if (!ends_with(req_str, "\r\n\r\n"))
        return;
      // We only check whether it's a GET request for /metrics for HTTP 1.x.
      // Everything else, we ignore for now.
      if (!starts_with(req_str, "GET /metrics HTTP/1.")) {
        write(msg.handle, as_bytes(make_span(request_not_supported)));
        flush_and_close();
        return;
      }
      // Collect metrics, ship response, and close.
      scrape();
      auto hdr = as_bytes(make_span(request_ok));
      auto text = collector_.collect_from(system().metrics());
      auto payload = as_bytes(make_span(text));
      auto& dst = wr_buf(msg.handle);
      dst.insert(dst.end(), hdr.begin(), hdr.end());
      dst.insert(dst.end(), payload.begin(), payload.end());
      flush_and_close();
    },
    [this](const io::new_connection_msg& msg) {
      // Pre-allocate buffer for maximum request size.
      auto& req = requests_[msg.handle];
      req.reserve(max_request_size);
      configure_read(msg.handle, io::receive_policy::at_most(1024));
    },
    [this](const io::connection_closed_msg& msg) {
      requests_.erase(msg.handle);
      if (num_connections() + num_doormen() == 0)
        quit();
    },
    [this](const io::acceptor_closed_msg&) {
      CAF_LOG_ERROR("Prometheus Broker lost its acceptor!");
      if (num_connections() + num_doormen() == 0)
        quit();
    },
  };
}

void prometheus_broker::scrape() {
  // Scrape system metrics at most once per second.
  auto now = time(NULL);
  if (last_scrape_ < now) {
    last_scrape_ = now;
    proc_importer_.update();
  }
}

} // namespace caf::detail
