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

#include "caf/attach_continuous_stream_stage.hpp"

#include "core-test.hpp"

#include <memory>
#include <numeric>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/attach_stream_sink.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/stateful_actor.hpp"

using std::string;

using namespace caf;

namespace {

/// Returns the sum of natural numbers up until `n`, i.e., 1 + 2 + ... + n.
int32_t sum(int32_t n) {
  return (n * (n + 1)) / 2;
}

TESTEE_SETUP();

TESTEE_STATE(file_reader) {
  std::vector<int32_t> buf;
};

VARARGS_TESTEE(file_reader, size_t buf_size) {
  return {[=](string& fname) -> result<stream<int32_t>, string> {
    CAF_CHECK_EQUAL(fname, "numbers.txt");
    CAF_CHECK_EQUAL(self->mailbox().empty(), true);
    return attach_stream_source(
      self,
      // forward file name in handshake to next stage
      std::forward_as_tuple(std::move(fname)),
      // initialize state
      [=](unit_t&) {
        auto& xs = self->state.buf;
        xs.resize(buf_size);
        std::iota(xs.begin(), xs.end(), 1);
      },
      // get next element
      [=](unit_t&, downstream<int32_t>& out, size_t num) {
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
  }};
}

TESTEE_STATE(sum_up) {
  int32_t x = 0;
};

TESTEE(sum_up) {
  return {[=](stream<int32_t>& in, const string& fname) {
            CAF_CHECK_EQUAL(fname, "numbers.txt");
            using int_ptr = int32_t*;
            return attach_stream_sink(
              self,
              // input stream
              in,
              // initialize state
              [=](int_ptr& x) { x = &self->state.x; },
              // processing step
              [](int_ptr& x, int32_t y) { *x += y; },
              // cleanup
              [=](int_ptr&, const error&) {
                CAF_MESSAGE(self->name() << " is done");
              });
          },
          [=](join_atom atm, actor src) {
            CAF_MESSAGE(self->name() << " joins a stream");
            self->send(self * src, atm);
          }};
}

TESTEE_STATE(stream_multiplexer) {
  stream_stage_ptr<int32_t, broadcast_downstream_manager<int32_t>> stage;
};

TESTEE(stream_multiplexer) {
  self->state.stage = attach_continuous_stream_stage(
    self,
    // initialize state
    [](unit_t&) {
      // nop
    },
    // processing step
    [](unit_t&, downstream<int32_t>& out, int32_t x) { out.push(x); },
    // cleanup
    [=](unit_t&, const error&) { CAF_MESSAGE(self->name() << " is done"); });
  return {
    [=](join_atom) {
      CAF_MESSAGE("received 'join' request");
      return self->state.stage->add_outbound_path(
        std::make_tuple("numbers.txt"));
    },
    [=](const stream<int32_t>& in, std::string& fname) {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      return self->state.stage->add_inbound_path(in);
    },
    [=](close_atom, int32_t sink_index) {
      auto& out = self->state.stage->out();
      out.close(out.path_slots().at(static_cast<size_t>(sink_index)));
    },
  };
}

using fixture = test_coordinator_fixture<>;

} // namespace

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(local_streaming_tests, fixture)

CAF_TEST(depth_3_pipeline_with_fork) {
  auto src = sys.spawn(file_reader, 60u);
  auto stg = sys.spawn(stream_multiplexer);
  auto snk1 = sys.spawn(sum_up);
  auto snk2 = sys.spawn(sum_up);
  auto& st = deref<stream_multiplexer_actor>(stg).state;
  CAF_MESSAGE("connect sinks to the stage (fork)");
  self->send(snk1, join_atom_v, stg);
  self->send(snk2, join_atom_v, stg);
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
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk1).state.x, sum(60));
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk2).state.x, sum(60));
  self->send_exit(stg, exit_reason::kill);
}

CAF_TEST(depth_3_pipeline_with_join) {
  auto src1 = sys.spawn(file_reader, 60u);
  auto src2 = sys.spawn(file_reader, 60u);
  auto stg = sys.spawn(stream_multiplexer);
  auto snk = sys.spawn(sum_up);
  auto& st = deref<stream_multiplexer_actor>(stg).state;
  CAF_MESSAGE("connect sink to the stage");
  self->send(snk, join_atom_v, stg);
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
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, sum(60) * 2);
  self->send_exit(stg, exit_reason::kill);
}

CAF_TEST_FIXTURE_SCOPE_END()
