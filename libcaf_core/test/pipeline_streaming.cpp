// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE pipeline_streaming

#include "core-test.hpp"

#include <memory>
#include <numeric>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/attach_stream_sink.hpp"
#include "caf/attach_stream_stage.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/stateful_actor.hpp"

using std::string;

using namespace caf;

namespace {

TESTEE_SETUP();

using buf = std::deque<int>;

template <class T, class Self>
auto fin(Self* self) {
  return [=](T&, const error& err) {
    self->state.fin_called += 1;
    if (err == none) {
      CAF_MESSAGE(self->name() << " is done");
    } else {
      CAF_MESSAGE(self->name() << " aborted with error");
    }
  };
}

TESTEE_STATE(infinite_source) {
  int fin_called = 0;
};

TESTEE(infinite_source) {
  return {
    [=](string& fname) -> result<stream<int>> {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      CAF_CHECK_EQUAL(self->mailbox().empty(), true);
      return attach_stream_source(
        self, [](int& x) { x = 0; },
        [](int& x, downstream<int>& out, size_t num) {
          for (size_t i = 0; i < num; ++i)
            out.push(x++);
        },
        [](const int&) { return false; }, fin<int>(self));
    },
  };
}

TESTEE_STATE(file_reader) {
  int fin_called = 0;
};

VARARGS_TESTEE(file_reader, size_t buf_size) {
  auto init = [](size_t buf_size) {
    return [=](buf& xs) {
      xs.resize(buf_size);
      std::iota(xs.begin(), xs.end(), 1);
    };
  };
  auto push_from_buf = [](buf& xs, downstream<int>& out, size_t num) {
    CAF_MESSAGE("push " << num << " messages downstream");
    auto n = std::min(num, xs.size());
    for (size_t i = 0; i < n; ++i)
      out.push(xs[i]);
    xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
  };
  auto is_done = [self](const buf& xs) {
    if (xs.empty()) {
      CAF_MESSAGE(self->name() << " exhausted its buffer");
      return true;
    }
    return false;
  };
  return {
    [=](string& fname) -> result<stream<int>> {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      CAF_CHECK_EQUAL(self->mailbox().empty(), true);
      return attach_stream_source(self, init(buf_size), push_from_buf, is_done,
                                  fin<buf>(self));
    },
    [=](string& fname, actor next) {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      CAF_CHECK_EQUAL(self->mailbox().empty(), true);
      attach_stream_source(self, next, init(buf_size), push_from_buf, is_done,
                           fin<buf>(self));
    },
  };
}

TESTEE_STATE(sum_up) {
  int x = 0;
  int fin_called = 0;
};

TESTEE(sum_up) {
  using intptr = int*;
  return {
    [=](stream<int>& in) {
      return attach_stream_sink(
        self,
        // input stream
        in,
        // initialize state
        [=](intptr& x) { x = &self->state.x; },
        // processing step
        [](intptr& x, int y) { *x += y; }, fin<intptr>(self));
    },
  };
}

TESTEE_STATE(delayed_sum_up) {
  int x = 0;
  int fin_called = 0;
};

TESTEE(delayed_sum_up) {
  using intptr = int*;
  self->set_default_handler(skip);
  return {
    [=](ok_atom) {
      self->become([=](stream<int>& in) {
        self->set_default_handler(print_and_drop);
        return attach_stream_sink(
          self,
          // input stream
          in,
          // initialize state
          [=](intptr& x) { x = &self->state.x; },
          // processing step
          [](intptr& x, int y) { *x += y; },
          // cleanup
          fin<intptr>(self));
      });
    },
  };
}

TESTEE_STATE(broken_sink) {
  int fin_called = 0;
};

TESTEE(broken_sink) {
  CAF_IGNORE_UNUSED(self);
  return {
    [=](stream<int>&, const actor&) {
      // nop
    },
  };
}

TESTEE_STATE(filter) {
  int fin_called = 0;
};

TESTEE(filter) {
  CAF_IGNORE_UNUSED(self);
  return {
    [=](stream<int>& in) {
      return attach_stream_stage(
        self,
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
        fin<unit_t>(self));
    },
  };
}

TESTEE_STATE(doubler) {
  int fin_called = 0;
};

TESTEE(doubler) {
  CAF_IGNORE_UNUSED(self);
  return {
    [=](stream<int>& in) {
      return attach_stream_stage(
        self,
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [](unit_t&, downstream<int>& out, int x) { out.push(x * 2); },
        // cleanup
        fin<unit_t>(self));
    },
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

} // namespace

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
  expect((upstream_msg::ack_batch), from(snk).to(src));
  expect((downstream_msg::close), from(src).to(snk));
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 1275);
  CAF_MESSAGE("verify that each actor called its finalizer once");
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<file_reader_actor>(src).state.fin_called, 1);
}

CAF_TEST(depth_2_pipeline_setup2_50_items) {
  auto src = sys.spawn(file_reader, 50u);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(src, "numbers.txt", snk);
  expect((string, actor), from(self).to(src).with("numbers.txt", snk));
  expect((open_stream_msg), to(snk));
  expect((upstream_msg::ack_open), from(snk).to(src));
  CAF_MESSAGE("start data transmission (a single batch)");
  expect((downstream_msg::batch), from(src).to(snk));
  expect((upstream_msg::ack_batch), from(snk).to(src));
  expect((downstream_msg::close), from(src).to(snk));
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 1275);
  CAF_MESSAGE("verify that each actor called its finalizer once");
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<file_reader_actor>(src).state.fin_called, 1);
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
  self->send(snk, ok_atom_v);
  expect((ok_atom), from(self).to(snk));
  expect((open_stream_msg), from(self).to(snk));
  expect((upstream_msg::ack_open), from(snk).to(src));
  CAF_MESSAGE("start data transmission (a single batch)");
  expect((downstream_msg::batch), from(src).to(snk));
  expect((upstream_msg::ack_batch), from(snk).to(src));
  CAF_MESSAGE("expect close message from src and then result from snk");
  expect((downstream_msg::close), from(src).to(snk));
  CAF_CHECK_EQUAL(deref<delayed_sum_up_actor>(snk).state.x, 1275);
  CAF_MESSAGE("verify that each actor called its finalizer once");
  CAF_CHECK_EQUAL(deref<delayed_sum_up_actor>(snk).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<file_reader_actor>(src).state.fin_called, 1);
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
  CAF_MESSAGE("verify that each actor called its finalizer once");
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<file_reader_actor>(src).state.fin_called, 1);
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
  run();
  CAF_MESSAGE("verify that the file reader called its finalizer once");
  CAF_CHECK_EQUAL(deref<file_reader_actor>(src).state.fin_called, 1);
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
  expect((downstream_msg::batch), from(src).to(snk));
  expect((downstream_msg::batch), from(src).to(snk));
  expect((downstream_msg::batch), from(src).to(snk));
  expect((downstream_msg::forced_close), from(_).to(snk));
  CAF_MESSAGE("verify that the sink called its finalizer once");
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.fin_called, 1);
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
  CAF_MESSAGE("verify that the source called its finalizer once");
  CAF_CHECK_EQUAL(deref<file_reader_actor>(src).state.fin_called, 1);
}

CAF_TEST(depth_3_pipeline_50_items) {
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
  CAF_MESSAGE("start data transmission (a single batch)");
  expect((downstream_msg::batch), from(src).to(stg));
  CAF_MESSAGE("the stage should delay its first batch since its underfull");
  disallow((downstream_msg::batch), from(stg).to(snk));
  CAF_MESSAGE("after running the pipeline the sink received all batches");
  run();
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.x, 625);
  CAF_CHECK_EQUAL(deref<file_reader_actor>(src).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<filter_actor>(stg).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.fin_called, 1);
}

CAF_TEST(depth_4_pipeline_500_items) {
  auto src = sys.spawn(file_reader, 500u);
  auto stg1 = sys.spawn(filter);
  auto stg2 = sys.spawn(doubler);
  auto snk = sys.spawn(sum_up);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(stg1) << CAF_ARG(stg2)
                            << CAF_ARG(snk));
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
  CAF_MESSAGE("verify that each actor called its finalizer once");
  CAF_CHECK_EQUAL(deref<file_reader_actor>(src).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<filter_actor>(stg1).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<doubler_actor>(stg2).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.fin_called, 1);
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
  CAF_MESSAGE("verify that each actor called its finalizer once");
  CAF_CHECK_EQUAL(deref<file_reader_actor>(src).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<filter_actor>(stg).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.fin_called, 1);
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
  CAF_MESSAGE("verify that each actor called its finalizer once");
  CAF_CHECK_EQUAL(deref<filter_actor>(stg).state.fin_called, 1);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk).state.fin_called, 1);
}

CAF_TEST_FIXTURE_SCOPE_END()
