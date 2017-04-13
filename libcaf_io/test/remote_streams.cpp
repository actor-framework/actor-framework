/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <string>
#include <numeric>
#include <fstream>
#include <iostream>
#include <iterator>

#define CAF_SUITE io_remote_streams
#include "caf/test/io_dsl.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/test_multiplexer.hpp"

using std::cout;
using std::endl;
using std::string;

using namespace caf;
using namespace caf::io;

namespace {

behavior drop_all(event_based_actor* self) {
  return {
    [=](stream<int>& in, std::string& fname) {
      CAF_CHECK_EQUAL(fname, "test.txt");
      return self->add_sink(
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [](unit_t&, int) {
          // nop
        },
        // cleanup and produce void "result"
        [](unit_t&) {
          CAF_LOG_INFO("drop_all done");
        }
      );
    }
  };
}

void streamer_without_result(event_based_actor* self, const actor& dest) {
  CAF_LOG_INFO("streamer_without_result initialized");
  using buf = std::deque<int>;
  self->new_stream(
    // destination of the stream
    dest,
    // "file name" for the next stage
    std::make_tuple("test.txt"),
    // initialize state
    [&](buf& xs) {
      xs = buf{1, 2, 3, 4, 5, 6, 7, 8, 9};
    },
    // get next element
    [=](buf& xs, downstream<int>& out, size_t num) {
      auto n = std::min(num, xs.size());
      for (size_t i = 0; i < n; ++i)
        out.push(xs[i]);
      xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
    },
    // check whether we reached the end
    [=](const buf& xs) {
      return xs.empty();
    },
    // handle result of the stream
    [=](expected<void>) {
      // nop
    }
  );
}

class remoting_config : public actor_system_config {
public:
  remoting_config() {
    load<middleman, network::test_multiplexer>();
    add_message_type<stream<int>>("stream<int>");
    add_message_type<std::vector<int>>("vector<int>");
    middleman_detach_utility_actors = false;
  }
};

using sub_fixture = test_node_fixture<remoting_config>;

} // namespace <anonymous>

struct dsl_path_info {
  sub_fixture& host;
  actor receiver;
  dsl_path_info(sub_fixture& x, actor y)
      : host(x), receiver(std::move(y)) {
    // nop
  }
  dsl_path_info(sub_fixture& x, strong_actor_ptr y)
      : host(x), receiver(actor_cast<actor>(std::move(y))) {
    // nop
  }
};

#define expect_on_path(types, fields, ...)                                     \
  CAF_MESSAGE(">>> " << #types << " on path " << #__VA_ARGS__);                \
  {                                                                            \
    std::vector<dsl_path_info> path{{__VA_ARGS__}};                            \
    for (auto x : path) {                                                      \
      network_traffic();                                                       \
      expect_on(x.host, types, from(_).to(x.receiver).fields);                 \
    }                                                                          \
  }                                                                            \
  CAF_MESSAGE("<<< path done")

CAF_TEST_FIXTURE_SCOPE(netstreams, point_to_point_fixture<remoting_config>)

CAF_TEST(stream_crossing_the_wire) {
  CAF_MESSAGE("earth stream serv: " << to_string(earth.stream_serv));
  CAF_MESSAGE("mars stream serv: " << to_string(mars.stream_serv));
  // Connect the buffers of mars and earth to setup a pseudo-network.
  mars.peer = &earth;
  earth.peer = &mars;
  // Setup: earth (streamer_without_result) streams to mars (drop_all).
  CAF_MESSAGE("spawn drop_all sink on mars");
  auto sink = mars.sys.spawn(drop_all);
  // This test uses two fixtures for keeping state.
  earth.conn = connection_handle::from_int(1);
  mars.conn = connection_handle::from_int(2);
  mars.acc = accept_handle::from_int(3);
  // Run any initialization code.
  exec_all();
  // Prepare publish and remote_actor calls.
  CAF_MESSAGE("prepare connections on earth and mars");
  prepare_connection(mars, earth, "mars", 8080u);
  // Publish sink on mars.
  CAF_MESSAGE("publish sink on mars");
  mars.publish(sink, 8080);
  // Get a proxy on earth.
  CAF_MESSAGE("connect from earth to mars");
  auto proxy = earth.remote_actor("mars", 8080);
  CAF_MESSAGE("got proxy: " << to_string(proxy) << ", spawn streamer on earth");
  CAF_MESSAGE("establish remote stream paths");
  // Establish remote paths between the two stream servers. This step is
  // necessary to prevent a deadlock in `stream_serv::remote_stream_serv` due
  // to blocking communication with the BASP broker.
  anon_send(actor_cast<actor>(earth.stream_serv), connect_atom::value,
            mars.stream_serv->node());
  anon_send(actor_cast<actor>(mars.stream_serv), connect_atom::value,
            earth.stream_serv->node());
  exec_all();
  CAF_MESSAGE("start streaming");
  // Start streaming.
  auto source = earth.sys.spawn(streamer_without_result, proxy);
  earth.sched.run_once();
  // source ----('sys', stream_msg::open)----> earth.stream_serv
  expect_on(earth, (atom_value, stream_msg),
            from(source).to(earth.stream_serv).with(sys_atom::value, _));
  // --------------(stream_msg::open)-------------->
  //  earth.stream_serv -> mars.stream_serv -> sink
  expect_on_path(
    (stream_msg::open), with(_, _, _, _, _, false),
    {mars, mars.stream_serv}, {mars, sink});
  // mars.stream_serv --('sys', 'ok', 5)--> earth.stream_serv
  network_traffic();
  expect_on(earth, (atom_value, atom_value, int32_t),
            from(_).to(earth.stream_serv)
            .with(sys_atom::value, ok_atom::value, 5));
  // -----------------(stream_msg::ack_open)------------------>
  //  sink -> mars.stream_serv -> earth.stream_serv -> source
  expect_on_path(
    (stream_msg::ack_open), with(_, 5, _, false),
    {mars, mars.stream_serv}, {earth, earth.stream_serv}, {earth, source});
  // earth.stream_serv --('sys', 'ok', 5)--> mars.stream_serv
  network_traffic();
  expect_on(mars, (atom_value, atom_value, int32_t),
            from(_).to(mars.stream_serv)
            .with(sys_atom::value, ok_atom::value, 5));
  // -------------------(stream_msg::batch)------------------->
  //  source -> earth.stream_serv -> mars.stream_serv -> sink
  expect_on_path(
    (stream_msg::batch), with(5, std::vector<int>{1, 2, 3, 4, 5}, 0),
    {earth, earth.stream_serv},
    {mars, mars.stream_serv}, {mars, sink});
  // -----------------(stream_msg::ack_batch)------------------>
  //  sink -> mars.stream_serv -> earth.stream_serv -> source
  expect_on_path(
    (stream_msg::ack_batch), with(5, 0),
    {mars, mars.stream_serv}, {earth, earth.stream_serv}, {earth, source});
  // -------------------(stream_msg::batch)------------------->
  //  source -> earth.stream_serv -> mars.stream_serv -> sink
  expect_on_path(
    (stream_msg::batch), with(4, std::vector<int>{6, 7, 8, 9}, 1),
    {earth, earth.stream_serv},
    {mars, mars.stream_serv}, {mars, sink});
  // -----------------(stream_msg::ack_batch)------------------>
  //  sink -> mars.stream_serv -> earth.stream_serv -> source
  expect_on_path(
    (stream_msg::ack_batch), with(4, 1),
    {mars, mars.stream_serv}, {earth, earth.stream_serv}, {earth, source});
  // -------------------(stream_msg::close)------------------->
  //  source -> earth.stream_serv -> mars.stream_serv -> sink
  expect_on_path(
    (stream_msg::close), with(),
    {earth, earth.stream_serv},
    {mars, mars.stream_serv}, {mars, sink});
  // sink ----(result: <empty>)---> source
  network_traffic();
  expect_on(earth, (void), from(proxy).to(source).with());
  // sink --(void)--> source
  anon_send_exit(sink, exit_reason::user_shutdown);
  mars.sched.run();
  anon_send_exit(source, exit_reason::user_shutdown);
  earth.sched.run();
}

CAF_TEST_FIXTURE_SCOPE_END()
