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

TESTEE_SETUP();

VARARGS_TESTEE(file_reader, size_t buf_size) {
  using buf = std::deque<int>;
  return {
    [=](string& fname) -> output_stream<int, string> {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      CAF_CHECK_EQUAL(self->mailbox().empty(), true);
      return self->make_source(
        // forward file name in handshake to next stage
        std::forward_as_tuple(std::move(fname)),
        // initialize state
        [=](buf& xs) {
          xs.resize(buf_size);
          std::iota(xs.begin(), xs.end(), 1);
        },
        // get next element
        [](buf& xs, downstream<int>& out, size_t num) {
          CAF_MESSAGE("push " << num << " messages downstream");
          auto n = std::min(num, xs.size());
          for (size_t i = 0; i < n; ++i)
            out.push(xs[i]);
          xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
        },
        // check whether we reached the end
        [=](const buf& xs) {
          if (xs.empty()) {
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
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, int y) {
          self->state.x += y;
        },
        // cleanup and produce result message
        [=](unit_t&) -> int {
          CAF_MESSAGE(self->name() << " is done");
          return self->state.x;
        }
      );
    },
    [=](join_atom atm, actor src) {
      CAF_MESSAGE(self->name() << " joins a stream");
      self->send(self * src, atm);
    }
  };
}

TESTEE(delayed_sum_up) {
  self->set_default_handler(skip);
  return {
    [=](ok_atom) {
      self->become(
        [=](stream<int>& in, const std::string& fname) {
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
            [=](int& x) -> int {
              CAF_MESSAGE(self->name() << " is done");
              return x;
            }
          );
        }
      );
    }
  };
}

TESTEE(broken_sink) {
  CAF_IGNORE_UNUSED(self);
  return {
    [=](stream<int>&, const std::string& fname) {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
    }
  };
}

TESTEE(filter) {
  CAF_IGNORE_UNUSED(self);
  return {
    [=](stream<int>& in, std::string& fname) {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      return self->make_stage(
        // input stream
        in,
        // forward file name in handshake to next stage
        std::forward_as_tuple(std::move(fname)),
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [](unit_t&, downstream<int>& out, int x) {
          if ((x & 0x01) != 0)
            out.push(x);
        },
        // cleanup
        [=](unit_t&) {
          CAF_MESSAGE(self->name() << " is done");
        }
      );
    }
  };
}

TESTEE(doubler) {
  CAF_IGNORE_UNUSED(self);
  return {
    [=](stream<int>& in, std::string& fname) {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      return self->make_stage(
        // input stream
        in,
        // forward file name in handshake to next stage
        std::forward_as_tuple(std::move(fname)),
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [](unit_t&, downstream<int>& out, int x) {
          out.push(x * 2);
        },
        // cleanup
        [=](unit_t&) {
          CAF_MESSAGE(self->name() << " is done");
        }
      );
    }
  };
}

TESTEE_STATE(stream_multiplexer) {
  stream_stage_ptr<int, int, std::string> stage;
};

TESTEE(stream_multiplexer) {
  self->state.stage = self->make_continuous_stage(
    // handshake data
    std::make_tuple(std::string{"numbers.txt"}),
    // initialize state
    [](unit_t&) {
      // nop
    },
    // processing step
    [](unit_t&, downstream<int>& out, int x) {
      out.push(x);
    },
    // cleanup
    [=](unit_t&) {
      CAF_MESSAGE(self->name() << " is done");
    }
  );
  return {
    [=](join_atom) {
      CAF_MESSAGE("received 'join' request");
      return self->add_output_path(self->state.stage);
    },
    [=](const stream<int>& in, std::string& fname) {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      return self->add_input_path(in, self->state.stage);
    },
  };
}

struct fixture : test_coordinator_fixture<> {
  std::chrono::microseconds cycle;
  fixture() : cycle(cfg.streaming_credit_round_interval_us) {
    // Configure the clock to measure each batch item with 1us.
    sched.clock().time_per_unit.emplace(atom("batch"), timespan{1000});
    // Make sure the current time isn't invalid.
    sched.clock().current_time += cycle;
  }

  ~fixture() {
    sched.run();
  }
};

error fail_state(const actor& x) {
  auto ptr = actor_cast<abstract_actor*>(x);
  return dynamic_cast<monitorable_actor&>(*ptr).fail_state();
}

} // namespace <anonymous>

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(local_streaming_tests, fixture)

CAF_TEST(depth_2_pipeline_50_items) {
  auto src = sys.spawn(file_reader, 50);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(src));
  CAF_MESSAGE("start data transmission (a single batch)");
  expect((downstream_msg::batch), from(src).to(snk));
  sched.clock().current_time += cycle;
  sched.dispatch();
  expect((timeout_msg), from(snk).to(snk));
  expect((timeout_msg), from(src).to(src));
  expect((upstream_msg::ack_batch), from(snk).to(src));
  CAF_MESSAGE("expect close message from src and then result from snk");
  expect((downstream_msg::close), from(src).to(snk));
  expect((int), from(snk).to(self).with(1275));
  CAF_CHECK_EQUAL(fail_state(snk), exit_reason::normal);
  CAF_CHECK_EQUAL(fail_state(src), exit_reason::normal);
}

CAF_TEST(delayed_depth_2_pipeline_50_items) {
  auto src = sys.spawn(file_reader, 50);
  auto snk = sys.spawn(delayed_sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  disallow((upstream_msg::ack_open), from(snk).to(src));
  disallow((upstream_msg::forced_drop), from(snk).to(src));
  CAF_MESSAGE("send 'ok' to trigger sink to handle open_stream_msg");
  self->send(snk, ok_atom::value);
  expect((ok_atom), from(self).to(snk));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(src));
  CAF_MESSAGE("start data transmission (a single batch)");
  expect((downstream_msg::batch), from(src).to(snk));
  sched.clock().current_time += cycle;
  sched.dispatch();
  expect((timeout_msg), from(snk).to(snk));
  expect((timeout_msg), from(src).to(src));
  expect((upstream_msg::ack_batch), from(snk).to(src));
  CAF_MESSAGE("expect close message from src and then result from snk");
  expect((downstream_msg::close), from(src).to(snk));
  expect((int), from(snk).to(self).with(1275));
  CAF_CHECK_EQUAL(fail_state(snk), exit_reason::normal);
  CAF_CHECK_EQUAL(fail_state(src), exit_reason::normal);
}

CAF_TEST(depth_2_pipeline_500_items) {
  auto src = sys.spawn(file_reader, 500);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(src));
  CAF_MESSAGE("start data transmission (loop until src sends 'close')");
  do {
    CAF_MESSAGE("process all batches at the sink");
    while (received<downstream_msg::batch>(snk)) {
      expect((downstream_msg::batch), from(src).to(snk));
    }
    CAF_MESSAGE("trigger timeouts");
    sched.clock().current_time += cycle;
    sched.dispatch();
    expect((timeout_msg), from(snk).to(snk));
    expect((timeout_msg), from(src).to(src));
    CAF_MESSAGE("process ack_batch in source");
    expect((upstream_msg::ack_batch), from(snk).to(src));
  } while (!received<downstream_msg::close>(snk));
  CAF_MESSAGE("expect close message from src and then result from snk");
  expect((downstream_msg::close), from(src).to(snk));
  expect((int), from(snk).to(self).with(125250));
  CAF_CHECK_EQUAL(fail_state(snk), exit_reason::normal);
  CAF_CHECK_EQUAL(fail_state(src), exit_reason::normal);
}

CAF_TEST(depth_2_pipeline_error_during_handshake) {
  CAF_MESSAGE("streams must abort if a sink fails to initialize its state");
  auto src = sys.spawn(file_reader, 50);
  auto snk = sys.spawn(broken_sink);
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((std::string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::forced_drop), from(snk).to(src));
  expect((error), from(snk).to(self).with(sec::stream_init_failed));
}

CAF_TEST(depth_2_pipeline_error_at_source) {
  CAF_MESSAGE("streams must abort if a source fails at runtime");
  auto src = sys.spawn(file_reader, 500);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(src));
  CAF_MESSAGE("start data transmission (and abort source)");
  self->send_exit(src, exit_reason::kill);
  expect((downstream_msg::batch), from(src).to(snk));
  expect((exit_msg), from(self).to(src));
  CAF_MESSAGE("expect close message from src and then result from snk");
  expect((downstream_msg::forced_close), from(src).to(snk));
  expect((error), from(snk).to(self));
  CAF_CHECK_EQUAL(fail_state(snk), exit_reason::normal);
  CAF_CHECK_EQUAL(fail_state(src), exit_reason::kill);
}

CAF_TEST(depth_2_pipelin_error_at_sink) {
  CAF_MESSAGE("streams must abort if a sink fails at runtime");
  auto src = sys.spawn(file_reader, 500);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  CAF_MESSAGE("start data transmission (and abort sink)");
  self->send_exit(snk, exit_reason::kill);
  expect((upstream_msg::ack_open), from(snk).to(src));
  expect((exit_msg), from(self).to(snk));
  CAF_MESSAGE("expect close and result messages from snk");
  expect((upstream_msg::forced_drop), from(snk).to(src));
  expect((error), from(snk).to(self));
  CAF_CHECK_EQUAL(fail_state(src), exit_reason::normal);
  CAF_CHECK_EQUAL(fail_state(snk), exit_reason::kill);
}

CAF_TEST(depth_3_pipeline_50_items) {
  auto src = sys.spawn(file_reader, 50);
  auto stg = sys.spawn(filter);
  auto snk = sys.spawn(sum_up);
  auto next_cycle = [&] {
    sched.clock().current_time += cycle;
    sched.dispatch();
    expect((timeout_msg), from(snk).to(snk));
    expect((timeout_msg), from(stg).to(stg));
    expect((timeout_msg), from(src).to(src));
  };
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(stg) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * stg * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(stg));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(stg));
  expect((upstream_msg::ack_open), from(stg).to(src));
  CAF_MESSAGE("start data transmission (a single batch)");
  expect((downstream_msg::batch), from(src).to(stg));
  CAF_MESSAGE("the stage should delay its first batch since its underfull");
  disallow((downstream_msg::batch), from(stg).to(snk));
  next_cycle();
  CAF_MESSAGE("the source shuts down and the stage sends the final batch");
  expect((upstream_msg::ack_batch), from(stg).to(src));
  expect((downstream_msg::close), from(src).to(stg));
  expect((downstream_msg::batch), from(stg).to(snk));
  next_cycle();
  CAF_MESSAGE("the stage shuts down and the sink produces its final result");
  expect((upstream_msg::ack_batch), from(snk).to(stg));
  expect((downstream_msg::close), from(stg).to(snk));
  expect((int), from(snk).to(self).with(625));
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 625);
}

CAF_TEST(depth_3_pipeline_with_fork) {
  auto src = sys.spawn(file_reader, 50);
  auto stg = sys.spawn(stream_multiplexer);
  auto snk1 = sys.spawn(sum_up);
  auto snk2 = sys.spawn(sum_up);
  auto& st = deref<stream_multiplexer_actor>(stg).state;
  CAF_MESSAGE("connect sinks to the stage (fork)");
  self->send(snk1, join_atom::value, stg);
  self->send(snk2, join_atom::value, stg);
  sched.run();
  CAF_CHECK_EQUAL(st.stage->out().paths().size(), 2u);
  CAF_MESSAGE("connect source to the stage (fork)");
  self->send(stg * src, "numbers.txt");
  sched.run();
  CAF_CHECK_EQUAL(st.stage->out().paths().size(), 2u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 1u);
  auto predicate = [&] {
    return st.stage->inbound_paths().empty() && st.stage->out().clean();
  };
  sched.run_dispatch_loop(predicate, cycle);
  CAF_CHECK_EQUAL(st.stage->out().paths().size(), 2u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 0u);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk1).state.x, 1275);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk2).state.x, 1275);
  self->send_exit(stg, exit_reason::kill);
}

CAF_TEST(depth_3_pipeline_with_join) {
  auto src1 = sys.spawn(file_reader, 50);
  auto src2 = sys.spawn(file_reader, 50);
  auto stg = sys.spawn(stream_multiplexer);
  auto snk = sys.spawn(sum_up);
  auto& st = deref<stream_multiplexer_actor>(stg).state;
  CAF_MESSAGE("connect sink to the stage");
  self->send(snk, join_atom::value, stg);
  sched.run();
  CAF_CHECK_EQUAL(st.stage->out().paths().size(), 1u);
  CAF_MESSAGE("connect sources to the stage (join)");
  self->send(stg * src1, "numbers.txt");
  self->send(stg * src2, "numbers.txt");
  sched.run();
  CAF_CHECK_EQUAL(st.stage->out().paths().size(), 1u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 2u);
  auto predicate = [&] {
    return st.stage->inbound_paths().empty() && st.stage->out().clean();
  };
  sched.run_dispatch_loop(predicate, cycle);
  CAF_CHECK_EQUAL(st.stage->out().paths().size(), 1u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 0u);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 2550);
  self->send_exit(stg, exit_reason::kill);
}

CAF_TEST(depth_4_pipeline_500_items) {
  auto src = sys.spawn(file_reader, 500);
  auto stg1 = sys.spawn(filter);
  auto stg2 = sys.spawn(doubler);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(stg1)
              << CAF_ARG(stg2) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * stg2 * stg1 * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(stg1));
  expect((open_stream_msg), from(self).to(stg2));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(stg2));
  expect((upstream_msg::ack_open), from(stg2).to(stg1));
  expect((upstream_msg::ack_open), from(stg1).to(src));
  CAF_MESSAGE("start data transmission");
  sched.run_dispatch_loop(cycle);
  CAF_MESSAGE("check sink result");
  expect((int), from(snk).to(self).with(125000));
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 125000);
}

CAF_TEST_FIXTURE_SCOPE_END()
