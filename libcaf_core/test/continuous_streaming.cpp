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

#define CAF_SUITE continuous_streaming

#include "caf/test/dsl.hpp"

#include <memory>
#include <numeric>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/stateful_actor.hpp"

using std::string;

using namespace caf;

namespace {

/// Returns the sum of natural numbers up until `n`, i.e., 1 + 2 + ... + n.
int sum(int n) {
  return (n * (n + 1)) / 2;
}

TESTEE_SETUP();

TESTEE_STATE(file_reader) {
  std::vector<int> buf;
};

VARARGS_TESTEE(file_reader, size_t buf_size) {
  return {
    [=](string& fname) -> output_stream<int, string> {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      CAF_CHECK_EQUAL(self->mailbox().empty(), true);
      return self->make_source(
        // forward file name in handshake to next stage
        std::forward_as_tuple(std::move(fname)),
        // initialize state
        [=](unit_t&) {
          auto& xs = self->state.buf;
          xs.resize(buf_size);
          std::iota(xs.begin(), xs.end(), 1);
        },
        // get next element
        [=](unit_t&, downstream<int>& out, size_t num) {
          auto& xs = self->state.buf;
          CAF_MESSAGE("push " << num << " messages downstream");
          auto n = std::min(num, xs.size());
          for (size_t i = 0; i < n; ++i)
            out.push(xs[i]);
          xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
        },
        // check whether we reached the end
        [=](const unit_t&) {
          if (self->state.buf.empty()) {
            CAF_MESSAGE(self->name() << " is done");
            return true;
          }
          return false;
        });
    }
  };
}

TESTEE_STATE(sum_up) {
  int x = 0;
};

TESTEE(sum_up) {
  return {
    [=](stream<int>& in, const string& fname) {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      using int_ptr = int*;
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [=](int_ptr& x) {
          x = &self->state.x;
        },
        // processing step
        [](int_ptr& x , int y) {
          *x += y;
        },
        // cleanup
        [=](int_ptr&, const error&) {
          CAF_MESSAGE(self->name() << " is done");
        }
      );
    },
    [=](join_atom atm, actor src) {
      CAF_MESSAGE(self->name() << " joins a stream");
      self->send(self * src, atm);
    }
  };
}

TESTEE_STATE(stream_multiplexer) {
  stream_stage_ptr<int, broadcast_downstream_manager<int>> stage;
};

TESTEE(stream_multiplexer) {
  self->state.stage = self->make_continuous_stage(
    // initialize state
    [](unit_t&) {
      // nop
    },
    // processing step
    [](unit_t&, downstream<int>& out, int x) {
      out.push(x);
    },
    // cleanup
    [=](unit_t&, const error&) {
      CAF_MESSAGE(self->name() << " is done");
    }
  );
  return {
    [=](join_atom) {
      CAF_MESSAGE("received 'join' request");
      return self->state.stage->add_outbound_path(
        std::make_tuple("numbers.txt"));
    },
    [=](const stream<int>& in, std::string& fname) {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      return self->state.stage->add_inbound_path(in);
    },
    [=](close_atom, int sink_index) {
      auto& out = self->state.stage->out();
      out.close(out.path_slots().at(static_cast<size_t>(sink_index)));
    },
  };
}

using fixture = test_coordinator_fixture<>;

} // namespace <anonymous>

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(local_streaming_tests, fixture)

CAF_TEST(depth_3_pipeline_with_fork) {
  auto src = sys.spawn(file_reader, 50u);
  auto stg = sys.spawn(stream_multiplexer);
  auto snk1 = sys.spawn(sum_up);
  auto snk2 = sys.spawn(sum_up);
  auto& st = deref<stream_multiplexer_actor>(stg).state;
  CAF_MESSAGE("connect sinks to the stage (fork)");
  self->send(snk1, join_atom::value, stg);
  self->send(snk2, join_atom::value, stg);
  consume_messages();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 2u);
  CAF_MESSAGE("connect source to the stage (fork)");
  self->send(stg * src, "numbers.txt");
  consume_messages();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 2u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 1u);
  run();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 2u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 0u);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk1).state.x, 1275);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk2).state.x, 1275);
  self->send_exit(stg, exit_reason::kill);
}

CAF_TEST(depth_3_pipeline_with_join) {
  auto src1 = sys.spawn(file_reader, 50u);
  auto src2 = sys.spawn(file_reader, 50u);
  auto stg = sys.spawn(stream_multiplexer);
  auto snk = sys.spawn(sum_up);
  auto& st = deref<stream_multiplexer_actor>(stg).state;
  CAF_MESSAGE("connect sink to the stage");
  self->send(snk, join_atom::value, stg);
  consume_messages();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 1u);
  CAF_MESSAGE("connect sources to the stage (join)");
  self->send(stg * src1, "numbers.txt");
  self->send(stg * src2, "numbers.txt");
  consume_messages();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 1u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 2u);
  run();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 1u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 0u);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 2550);
  self->send_exit(stg, exit_reason::kill);
}

CAF_TEST(closing_downstreams_before_end_of_stream) {
  auto src = sys.spawn(file_reader, 10000u);
  auto stg = sys.spawn(stream_multiplexer);
  auto snk1 = sys.spawn(sum_up);
  auto snk2 = sys.spawn(sum_up);
  auto& st = deref<stream_multiplexer_actor>(stg).state;
  CAF_MESSAGE("connect sinks to the stage (fork)");
  self->send(snk1, join_atom::value, stg);
  self->send(snk2, join_atom::value, stg);
  consume_messages();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 2u);
  CAF_MESSAGE("connect source to the stage (fork)");
  self->send(stg * src, "numbers.txt");
  consume_messages();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 2u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 1u);
  CAF_MESSAGE("do a single round of credit");
  trigger_timeouts();
  consume_messages();
  CAF_MESSAGE("make sure the stream isn't done yet");
  CAF_REQUIRE(!deref<file_reader_actor>(src).state.buf.empty());
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 2u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 1u);
  CAF_MESSAGE("get the next not-yet-buffered integer");
  auto next_pending = deref<file_reader_actor>(src).state.buf.front();
  CAF_REQUIRE_GREATER(next_pending, 0);
  auto sink1_result = sum(next_pending - 1);
  CAF_MESSAGE("gracefully close sink 1, next pending: " << next_pending);
  self->send(stg, close_atom::value, 0);
  expect((atom_value, int), from(self).to(stg));
  CAF_MESSAGE("ship remaining elements");
  run();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 1u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 0u);
  CAF_CHECK_LESS(deref<sum_up_actor>(snk1).state.x, sink1_result);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk2).state.x, sum(10000));
  self->send_exit(stg, exit_reason::kill);
}

CAF_TEST_FIXTURE_SCOPE_END()
