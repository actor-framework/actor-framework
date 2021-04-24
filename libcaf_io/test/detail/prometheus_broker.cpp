// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.prometheus_broker

#include "caf/detail/prometheus_broker.hpp"

#include "io-test.hpp"

#include "caf/io/network/default_multiplexer.hpp"
#include "caf/policy/tcp.hpp"

using namespace caf;
using namespace caf::io;

namespace {

struct fixture : test_node_fixture<> {
  caf::actor aut;

  fixture() {
    using detail::prometheus_broker;
    actor_config cfg{&sys.middleman().backend()};
    aut = sys.spawn_impl<prometheus_broker, spawn_options::no_flags>(cfg);
    run();
    // assign the acceptor handle to the AUT
    auto ptr = static_cast<abstract_broker*>(actor_cast<abstract_actor*>(aut));
    ptr->add_doorman(mpx.new_doorman(acceptor, 1u));
    // "open" a new connection to our server
    mpx.add_pending_connect(acceptor, connection);
    mpx.accept_connection(acceptor);
  }

  ~fixture() {
    anon_send(aut, exit_reason::user_shutdown);
    run();
  }

  accept_handle acceptor = accept_handle::from_int(1);
  connection_handle connection = connection_handle::from_int(1);
};

bool contains(string_view str, string_view what) {
  return str.find(what) != string_view::npos;
}

constexpr string_view http_request
  = "GET /metrics HTTP/1.1\r\n"
    "Host: localhost:8090\r\n"
    "User-Agent: Prometheus/2.18.1\r\n"
    "Accept: application/openmetrics-text; "
    "version=0.0.1,text/plain;version=0.0.4;q=0.5,*/*;q=0.1\r\n"
    "Accept-Encoding: gzip\r\n"
    "X-Prometheus-Scrape-Timeout-Seconds: 5.000000\r\n\r\n";

constexpr string_view http_ok_header = "HTTP/1.1 200 OK\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "Connection: Closed\r\n\r\n";

} // namespace

CAF_TEST_FIXTURE_SCOPE(prometheus_broker_tests, fixture)

CAF_TEST(the prometheus broker responds to HTTP get requests) {
  auto bytes = as_bytes(make_span(http_request));
  mpx.virtual_send(connection, byte_buffer{bytes.begin(), bytes.end()});
  run();
  auto& response_buf = mpx.output_buffer(connection);
  string_view response{reinterpret_cast<char*>(response_buf.data()),
                       response_buf.size()};
  CAF_CHECK(starts_with(response, http_ok_header));
  CAF_CHECK(contains(response, "\ncaf_system_running_actors 2 "));
  if (detail::prometheus_broker::has_process_metrics()) {
    CAF_CHECK(contains(response, "\nprocess_cpu_seconds_total "));
    CAF_CHECK(contains(response, "\nprocess_resident_memory_bytes "));
    CAF_CHECK(contains(response, "\nprocess_virtual_memory_bytes "));
  }
}

CAF_TEST_FIXTURE_SCOPE_END()

namespace {

static constexpr size_t chunk_size = 1024;

using io::network::native_socket;

std::vector<char> read_all(string_view query, native_socket fd) {
  while (!query.empty()) {
    size_t written = 0;
    policy::tcp::write_some(written, fd, query.data(), query.size());
    query.remove_prefix(written);
  }
  std::vector<char> buf;
  char chunk[chunk_size];
  memset(chunk, 0, chunk_size);
  for (;;) {
    size_t received = 0;
    auto state = policy::tcp::read_some(received, fd, chunk, chunk_size);
    if (received > 0)
      buf.insert(buf.end(), chunk, chunk + received);
    if (state == io::network::rw_state::failure) {
      io::network::close_socket(fd);
      return buf;
    }
  }
}

std::vector<char> read_all(string_view query, const std::string& host,
                           uint16_t port) {
  if (auto fd = io::network::new_tcp_connection(host, port))
    return read_all(query, *fd);
  else
    FAIL("new_tcp_connection: " << to_string(fd.error()));
}

} // namespace

SCENARIO("setting caf.middleman.prometheus-http.port exports metrics to HTTP") {
  GIVEN("a config with an entry for caf.middleman.prometheus-http.port") {
    actor_system_config cfg;
    cfg.load<io::middleman>();
    cfg.set("caf.scheduler.max-threads", 2);
    cfg.set("caf.middleman.prometheus-http.port", 0);
    WHEN("starting an actor system using the config") {
      actor_system sys{cfg};
      THEN("the middleman creates a background task for HTTP requests") {
        auto scraping_port = sys.middleman().prometheus_scraping_port();
        REQUIRE_NE(scraping_port, 0);
        auto response_buf = read_all(http_request, "localhost", scraping_port);
        string_view response{reinterpret_cast<char*>(response_buf.data()),
                             response_buf.size()};
        CAF_CHECK(starts_with(response, http_ok_header));
        CAF_CHECK(contains(response, "\ncaf_system_running_actors "));
        if (detail::prometheus_broker::has_process_metrics()) {
          CAF_CHECK(contains(response, "\nprocess_cpu_seconds_total "));
          CAF_CHECK(contains(response, "\nprocess_resident_memory_bytes "));
          CAF_CHECK(contains(response, "\nprocess_virtual_memory_bytes "));
        }
      }
    }
  }
}
