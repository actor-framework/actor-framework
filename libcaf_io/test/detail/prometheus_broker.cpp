/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE detail.prometheus_broker

#include "caf/detail/prometheus_broker.hpp"

#include "caf/test/io_dsl.hpp"

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

} // namespace

CAF_TEST_FIXTURE_SCOPE(prometheus_broker_tests, fixture)

CAF_TEST(the prometheus broker responds to HTTP get requests) {
  string_view request
    = "GET /metrics HTTP/1.1\r\n"
      "Host: localhost:8090\r\n"
      "User-Agent: Prometheus/2.18.1\r\n"
      "Accept: application/openmetrics-text; "
      "version=0.0.1,text/plain;version=0.0.4;q=0.5,*/*;q=0.1\r\n"
      "Accept-Encoding: gzip\r\n"
      "X-Prometheus-Scrape-Timeout-Seconds: 5.000000\r\n\r\n";
  auto bytes = as_bytes(make_span(request));
  mpx.virtual_send(connection, byte_buffer{bytes.begin(), bytes.end()});
  run();
  auto& response_buf = mpx.output_buffer(connection);
  string_view response{reinterpret_cast<char*>(response_buf.data()),
                       response_buf.size()};
  string_view ok_header = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Connection: Closed\r\n\r\n";
  CAF_CHECK(starts_with(response, ok_header));
  CAF_CHECK(contains(response, "\ncaf_running_actors 2 "));
  if (detail::prometheus_broker::has_process_metrics()) {
    CAF_CHECK(contains(response, "\nprocess_cpu_seconds_total "));
    CAF_CHECK(contains(response, "\nprocess_resident_memory_bytes "));
    CAF_CHECK(contains(response, "\nprocess_virtual_memory_bytes "));
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
