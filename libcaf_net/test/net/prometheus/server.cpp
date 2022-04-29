// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// prometheuss://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.prometheus.server

#include "caf/net/prometheus/server.hpp"

#include "net-test.hpp"

#include "caf/net/http/server.hpp"
#include "caf/telemetry/metric_registry.hpp"

using namespace caf;
using namespace caf::net;
using namespace std::literals;

constexpr std::string_view request_str
  = "GET /metrics HTTP/1.1\r\n"
    "Host: localhost:8090\r\n"
    "User-Agent: Prometheus/2.18.1\r\n"
    "Accept: text/plain;version=0.0.4\r\n"
    "Accept-Encoding: gzip\r\n"
    "X-Prometheus-Scrape-Timeout-Seconds: 5.000000\r\n\r\n";

SCENARIO("the Prometheus server responds to requests with scrape results") {
  GIVEN("a valid Prometheus GET request") {
    WHEN("sending it to an prometheus server") {
      THEN("the Prometheus server responds with metrics text data") {
        telemetry::metric_registry registry;
        auto fb = registry.counter_singleton("foo", "bar", "test metric");
        auto bf = registry.counter_singleton("bar", "foo", "test metric");
        fb->inc(3);
        bf->inc(7);
        auto prom_state = prometheus::server::scrape_state::make(&registry);
        auto prom_serv = prometheus::server::make(prom_state);
        auto http_serv = net::http::server::make(std::move(prom_serv));
        auto serv = mock_stream_transport::make(std::move(http_serv));
        CHECK_EQ(serv->init(settings{}), error{});
        serv->push(request_str);
        CHECK_EQ(serv->handle_input(),
                 static_cast<ptrdiff_t>(request_str.size()));
        auto out = serv->output_as_str();
        CHECK(out.find("foo_bar 3"sv) != std::string_view::npos);
        CHECK(out.find("bar_foo 7"sv) != std::string_view::npos);
      }
    }
  }
}
