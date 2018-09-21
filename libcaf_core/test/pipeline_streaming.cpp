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

#define CAF_SUITE pipeline_streaming

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

TESTEE_SETUP();

using buf = std::deque<int>;

std::function<void(buf&)> init(size_t buf_size) {
  return [=](buf& xs) {
    xs.resize(buf_size);
    std::iota(xs.begin(), xs.end(), 1);
  };
}

void push_from_buf(buf& xs, downstream<int>& out, size_t num) {
  CAF_MESSAGE("push " << num << " messages downstream");
  auto n = std::min(num, xs.size());
  for (size_t i = 0; i < n; ++i)
    out.push(xs[i]);
  xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
}

std::function<bool(const buf&)> is_done(scheduled_actor* self) {
  return [=](const buf& xs) {
    if (xs.empty()) {
      CAF_MESSAGE(self->name() << " exhausted its buffer");
      return true;
    }
    return false;
  };
}

template <class T>
std::function<void (T&, const error&)> fin(scheduled_actor* self) {
  return [=](T&, const error& err) {
    if (err == none) {
      CAF_MESSAGE(self->name() << " is done");
    } else {
      CAF_MESSAGE(self->name() << " aborted with error");
    }
  };
}

TESTEE(infinite_source) {
  return {
    [=](string& fname) -> output_stream<int> {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      CAF_CHECK_EQUAL(self->mailbox().empty(), true);
      return self->make_source(
        [](int& x) {
          x = 0;
        },
        [](int& x, downstream<int>& out, size_t num) {
          for (size_t i = 0; i < num; ++i)
            out.push(x++);
        },
        [](const int&) {
          return false;
        },
        fin<int>(self)
      );
    }
  };
}

VARARGS_TESTEE(file_reader, size_t buf_size) {
  return {
    [=](string& fname) -> output_stream<int> {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      CAF_CHECK_EQUAL(self->mailbox().empty(), true);
      return self->make_source(
        init(buf_size),
        push_from_buf,
        is_done(self),
        fin<buf>(self)
      );
    },
    [=](string& fname, actor next) {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      CAF_CHECK_EQUAL(self->mailbox().empty(), true);
      self->make_source(
        next,
        init(buf_size),
        push_from_buf,
        is_done(self),
        fin<buf>(self)
      );
    }
  };
}

TESTEE_STATE(sum_up) {
  int x = 0;
};

TESTEE(sum_up) {
  using intptr = int*;
  return {
    [=](stream<int>& in) {
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [=](intptr& x) {
          x = &self->state.x;
        },
        // processing step
        [](intptr& x, int y) {
          *x += y;
        },
        fin<intptr>(self)
      );
    }
  };
}

TESTEE_STATE(delayed_sum_up) {
  int x = 0;
};

TESTEE(delayed_sum_up) {
  using intptr = int*;
  self->set_default_handler(skip);
  return {
    [=](ok_atom) {
      self->become(
        [=](stream<int>& in) {
          self->set_default_handler(print_and_drop);
          return self->make_sink(
            // input stream
            in,
            // initialize state
            [=](intptr& x) {
              x = &self->state.x;
            },
            // processing step
            [](intptr& x, int y) {
              *x += y;
            },
            // cleanup
            fin<intptr>(self)
          );
        }
      );
    }
  };
}

TESTEE(broken_sink) {
  CAF_IGNORE_UNUSED(self);
  return {
    [=](stream<int>&, const actor&) {
      // nop
    }
  };
}

TESTEE(filter) {
  CAF_IGNORE_UNUSED(self);
  return {
    [=](stream<int>& in) {
      return self->make_stage(
        // input stream
        in,
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
        fin<unit_t>(self)
      );
    }
  };
}

TESTEE(doubler) {
  CAF_IGNORE_UNUSED(self);
  return {
    [=](stream<int>& in) {
      return self->make_stage(
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [](unit_t&, downstream<int>& out, int x) {
          out.push(x * 2);
        },
        // cleanup
        fin<unit_t>(self)
      );
    }
  };
}

struct fixture : test_coordinator_fixture<> {
  void tick() {
    advance_time(cfg.stream_credit_round_interval);
  }

  /// Simulate a hard error on an actor such as an uncaught exception or a
  /// disconnect from a remote actor.
  void hard_kill(const actor& x) {
    deref(x).cleanup(exit_reason::kill, nullptr);
  }
};

} // namespace <anonymous>

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(local_streaming_tests, fixture)

CAF_TEST(depth_2_pipeline_50_items) {
  auto src = sys.spawn(file_reader, 50u);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(src));
  CAF_MESSAGE("start data transmission (a single batch)");
  expect((downstream_msg::batch), from(src).to(snk));
  tick();
  expect((timeout_msg), from(snk).to(snk));
  expect((timeout_msg), from(src).to(src));
  expect((upstream_msg::ack_batch), from(snk).to(src));
  CAF_MESSAGE("expect close message from src and then result from snk");
  expect((downstream_msg::close), from(src).to(snk));
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 1275);
}

CAF_TEST(depth_2_pipeline_setup2_50_items) {
  auto src = sys.spawn(file_reader, 50u);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(src, "numbers.txt", snk);
  expect((string, actor), from(self).to(src).with("numbers.txt", snk));
  expect((open_stream_msg), from(strong_actor_ptr{nullptr}).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(src));
  CAF_MESSAGE("start data transmission (a single batch)");
  expect((downstream_msg::batch), from(src).to(snk));
  tick();
  expect((timeout_msg), from(snk).to(snk));
  expect((timeout_msg), from(src).to(src));
  expect((upstream_msg::ack_batch), from(snk).to(src));
  CAF_MESSAGE("expect close message from src and then result from snk");
  expect((downstream_msg::close), from(src).to(snk));
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 1275);
}

CAF_TEST(delayed_depth_2_pipeline_50_items) {
  auto src = sys.spawn(file_reader, 50u);
  auto snk = sys.spawn(delayed_sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  disallow((upstream_msg::ack_open), from(snk).to(src));
  disallow((upstream_msg::forced_drop), from(_).to(src));
  CAF_MESSAGE("send 'ok' to trigger sink to handle open_stream_msg");
  self->send(snk, ok_atom::value);
  expect((ok_atom), from(self).to(snk));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(src));
  CAF_MESSAGE("start data transmission (a single batch)");
  expect((downstream_msg::batch), from(src).to(snk));
  tick();
  expect((timeout_msg), from(snk).to(snk));
  expect((timeout_msg), from(src).to(src));
  expect((upstream_msg::ack_batch), from(snk).to(src));
  CAF_MESSAGE("expect close message from src and then result from snk");
  expect((downstream_msg::close), from(src).to(snk));
  CAF_CHECK_EQUAL(deref<delayed_sum_up_actor>(snk).state.x, 1275);
}

CAF_TEST(depth_2_pipeline_500_items) {
  auto src = sys.spawn(file_reader, 500u);
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
    tick();
    allow((timeout_msg), from(snk).to(snk));
    allow((timeout_msg), from(src).to(src));
    CAF_MESSAGE("process ack_batch in source");
    expect((upstream_msg::ack_batch), from(snk).to(src));
  } while (!received<downstream_msg::close>(snk));
  CAF_MESSAGE("expect close message from src and then result from snk");
  expect((downstream_msg::close), from(src).to(snk));
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 125250);
}

CAF_TEST(depth_2_pipeline_error_during_handshake) {
  CAF_MESSAGE("streams must abort if a sink fails to initialize its state");
  auto src = sys.spawn(file_reader, 50u);
  auto snk = sys.spawn(broken_sink);
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((std::string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::forced_drop), from(_).to(src));
  expect((error), from(snk).to(self).with(sec::stream_init_failed));
}

CAF_TEST(depth_2_pipeline_error_at_source) {
  CAF_MESSAGE("streams must abort if a source fails at runtime");
  auto src = sys.spawn(file_reader, 500u);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(src));
  CAF_MESSAGE("start data transmission (and abort source)");
  hard_kill(src);
  expect((downstream_msg::batch), from(src).to(snk));
  expect((downstream_msg::forced_close), from(_).to(snk));
}

CAF_TEST(depth_2_pipelin_error_at_sink) {
  CAF_MESSAGE("streams must abort if a sink fails at runtime");
  auto src = sys.spawn(file_reader, 500u);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(snk));
  CAF_MESSAGE("start data transmission (and abort sink)");
  hard_kill(snk);
  expect((upstream_msg::ack_open), from(snk).to(src));
  expect((upstream_msg::forced_drop), from(_).to(src));
}

CAF_TEST(depth_3_pipeline_50_items) {
  auto src = sys.spawn(file_reader, 50u);
  auto stg = sys.spawn(filter);
  auto snk = sys.spawn(sum_up);
  auto next_cycle = [&] {
    tick();
    allow((timeout_msg), from(snk).to(snk));
    allow((timeout_msg), from(stg).to(stg));
    allow((timeout_msg), from(src).to(src));
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
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 625);
}

CAF_TEST(depth_4_pipeline_500_items) {
  auto src = sys.spawn(file_reader, 500u);
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
  run();
  CAF_MESSAGE("check sink result");
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 125000);
}

CAF_TEST(depth_3_pipeline_graceful_shutdown) {
  auto src = sys.spawn(file_reader, 50u);
  auto stg = sys.spawn(filter);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(stg) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * stg * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(stg));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(stg));
  expect((upstream_msg::ack_open), from(stg).to(src));
  CAF_MESSAGE("start data transmission (a single batch) and stop the stage");
  anon_send_exit(stg, exit_reason::user_shutdown);
  CAF_MESSAGE("expect the stage to still transfer pending items to the sink");
  run();
  CAF_MESSAGE("check sink result");
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 625);
}

CAF_TEST(depth_3_pipeline_infinite_source) {
  auto src = sys.spawn(infinite_source);
  auto stg = sys.spawn(filter);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(stg) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * stg * src, "numbers.txt");
  expect((string), from(self).to(src).with("numbers.txt"));
  expect((open_stream_msg), from(self).to(stg));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(stg));
  expect((upstream_msg::ack_open), from(stg).to(src));
  CAF_MESSAGE("send exit to the source and expect the stream to terminate");
  anon_send_exit(src, exit_reason::user_shutdown);
  run();
}

CAF_TEST_FIXTURE_SCOPE_END()
