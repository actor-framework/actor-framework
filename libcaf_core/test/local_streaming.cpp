/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE local_streaming

#include "caf/test/dsl.hpp"

#include <memory>
#include <numeric>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/stateful_actor.hpp"

using std::string;
using std::vector;

using namespace caf;

namespace {

struct file_reader_state {
  static const char* name;
};

const char* file_reader_state::name = "file_reader";

behavior file_reader(stateful_actor<file_reader_state>* self, size_t buf_size) {
  using buf = std::deque<int>;
  return {
    [=](string& fname) -> output_stream<int, string> {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      CAF_CHECK_EQUAL(self->mailbox().empty(), true);
      return self->make_source(
        // forward file name in handshake to next stage
        std::forward_as_tuple(std::move(fname)),
        // initialize state
        [&](buf& xs) {
          xs.resize(buf_size);
          std::iota(xs.begin(), xs.end(), 0);
        },
        // get next element
        [=](buf& xs, downstream<int>& out, size_t num) {
          CAF_MESSAGE("push " << num << " more messages downstream");
          auto n = std::min(num, xs.size());
          for (size_t i = 0; i < n; ++i)
            out.push(xs[i]);
          xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
        },
        // check whether we reached the end
        [=](const buf& xs) {
          return xs.empty();
        }
      );
    }
  };
}

struct sum_up_state {
  static const char* name;
};

const char* sum_up_state::name = "sum_up";

behavior sum_up(stateful_actor<sum_up_state>* self) {
  return {
    [=](stream<int>& in, string& fname) {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [](int& x) {
          x = 0;
        },
        // processing step
        [](int& x, int y) {
          x += y;
        },
        // cleanup and produce result message
        [](int& x) -> int {
          return x;
        }
      );
    }
  };
}

struct fixture : test_coordinator_fixture<> {

};

error fail_state(const actor& x) {
  auto ptr = actor_cast<abstract_actor*>(x);
  return dynamic_cast<monitorable_actor&>(*ptr).fail_state();
}

} // namespace <anonymous>

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(local_streaming_tests, fixture)

CAF_TEST(depth_2_pipeline) {
  std::chrono::microseconds cycle{cfg.streaming_credit_round_interval_us};
  sched.clock().current_time += cycle;
  auto src = sys.spawn(file_reader, 10);
  auto snk = sys.spawn(sum_up);
  auto pipeline = snk * src;
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  // self --("numbers.txt")-----------> source                             sink
  self->send(pipeline, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  // self                               source --(open_stream_msg)-------> sink
  expect((open_stream_msg), from(self).to(snk));
  // self                               source <--------------(ack_open)-- sink
  expect((upstream_msg::ack_open), from(snk).to(src));
  // self                               source --(batch)-----------------> sink
  expect((downstream_msg::batch), from(src).to(snk));
  // self                               source <-------------(ack_batch)-- sink
  sched.clock().current_time += cycle;
  sched.dispatch();
  expect((timeout_msg), from(snk).to(snk));
  expect((timeout_msg), from(src).to(src));
  expect((upstream_msg::ack_batch), from(snk).to(src));
  // self                               source --(close)-----------------> sink
  expect((downstream_msg::close), from(src).to(snk));
  // self <-----------------------------------------------------(result)-- sink
  expect((int), from(snk).to(self).with(45));
  CAF_CHECK_EQUAL(fail_state(snk), exit_reason::normal);
  CAF_CHECK_EQUAL(fail_state(src), exit_reason::normal);
}

CAF_TEST_FIXTURE_SCOPE_END()
