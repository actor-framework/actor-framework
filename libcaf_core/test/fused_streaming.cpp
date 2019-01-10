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

#define CAF_SUITE fused_streaming

#include "caf/test/dsl.hpp"

#include <memory>
#include <numeric>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/fused_downstream_manager.hpp"
#include "caf/stateful_actor.hpp"

using std::string;

using namespace caf;

namespace {

TESTEE_SETUP();

using int_downstream_manager = broadcast_downstream_manager<int>;

using string_downstream_manager = broadcast_downstream_manager<string>;

using ints_atom = atom_constant<atom("ints")>;

using strings_atom = atom_constant<atom("strings")>;

template <class T>
void push(std::deque<T>& xs, downstream<T>& out, size_t num) {
  auto n = std::min(num, xs.size());
  CAF_MESSAGE("push " << n << " messages downstream");
  for (size_t i = 0; i < n; ++i)
    out.push(xs[i]);
  xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
}

VARARGS_TESTEE(int_file_reader, size_t buf_size) {
  using buf = std::deque<int32_t>;
  return {
    [=](string& fname) -> output_stream<int32_t> {
      CAF_CHECK_EQUAL(fname, "numbers.txt");
      return self->make_source(
        // initialize state
        [=](buf& xs) {
          xs.resize(buf_size);
          std::iota(xs.begin(), xs.end(), 1);
        },
        // get next element
        [](buf& xs, downstream<int32_t>& out, size_t num) {
          push(xs, out, num);
        },
        // check whether we reached the end
        [=](const buf& xs) {
          return xs.empty();
        });
    }
  };
}

VARARGS_TESTEE(string_file_reader, size_t buf_size) {
  using buf = std::deque<string>;
  return {
    [=](string& fname) -> output_stream<string> {
      CAF_CHECK_EQUAL(fname, "strings.txt");
      return self->make_source(
        // initialize state
        [=](buf& xs) {
          for (size_t i = 0; i < buf_size; ++i)
            xs.emplace_back("some string data");
        },
        // get next element
        [](buf& xs, downstream<string>& out, size_t num) {
          push(xs, out, num);
        },
        // check whether we reached the end
        [=](const buf& xs) {
          return xs.empty();
        });
    }
  };
}

TESTEE_STATE(sum_up) {
  int x = 0;
};

TESTEE(sum_up) {
  using intptr = int*;
  return {
    [=](stream<int32_t>& in) {
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [=](intptr& x) {
          x = &self->state.x;
        },
        // processing step
        [](intptr& x, int32_t y) {
          *x += y;
        },
        // cleanup and produce result message
        [=](intptr&, const error&) {
          CAF_MESSAGE(self->name() << " is done");
        }
      );
    },
    [=](join_atom, actor src) {
      CAF_MESSAGE(self->name() << " joins a stream");
      self->send(self * src, join_atom::value, ints_atom::value);
    }
  };
}

TESTEE_STATE(collect) {
  std::vector<string> strings;
};

TESTEE(collect) {
  return {
    [=](stream<string>& in) {
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, string y) {
          self->state.strings.emplace_back(std::move(y));
        },
        // cleanup and produce result message
        [=](unit_t&, const error&) {
          CAF_MESSAGE(self->name() << " is done");
        }
      );
    },
    [=](join_atom, actor src) {
      CAF_MESSAGE(self->name() << " joins a stream");
      self->send(self * src, join_atom::value, strings_atom::value);
    }
  };
}

using int_downstream_manager = broadcast_downstream_manager<int>;

using string_downstream_manager = broadcast_downstream_manager<string>;

using fused_manager =
  fused_downstream_manager<int_downstream_manager, string_downstream_manager>;

class fused_stage : public stream_manager {
public:
  using super = stream_manager;

  fused_stage(scheduled_actor* self) : stream_manager(self), out_(this) {
    continuous(true);
  }

  bool done() const override {
    return !continuous() && pending_handshakes_ == 0 && inbound_paths_.empty()
           && out_.clean();
  }

  bool idle() const noexcept override {
    return inbound_paths_idle() && out_.stalled();
  }

  void handle(inbound_path*, downstream_msg::batch& batch) override {
    using std::make_move_iterator;
    using int_vec = std::vector<int>;
    using string_vec = std::vector<string>;
    if (batch.xs.match_elements<int_vec>()) {
      CAF_MESSAGE("handle an integer batch");
      auto& xs = batch.xs.get_mutable_as<int_vec>(0);
      auto& buf = out_.get<int_downstream_manager>().buf();
      buf.insert(buf.end(), xs.begin(), xs.end());
      return;
    }
    if (batch.xs.match_elements<string_vec>()) {
      CAF_MESSAGE("handle a string batch");
      auto& xs = batch.xs.get_mutable_as<string_vec>(0);
      auto& buf = out_.get<string_downstream_manager>().buf();
      buf.insert(buf.end(), xs.begin(), xs.end());
      return;
    }
    CAF_LOG_ERROR("received unexpected batch type (dropped)");
  }

  bool congested() const noexcept override {
    return out_.capacity() == 0;
  }

  fused_manager& out() noexcept override {
    return out_;
  }

private:
  fused_manager out_;
};

TESTEE_STATE(stream_multiplexer) {
  intrusive_ptr<fused_stage> stage;
};

TESTEE(stream_multiplexer) {
  self->state.stage = make_counted<fused_stage>(self);
  return {
    [=](join_atom, ints_atom) {
      auto& stg = self->state.stage;
      CAF_MESSAGE("received 'join' request for integers");
      auto result = stg->add_unchecked_outbound_path<int>();
      stg->out().assign<int_downstream_manager>(result);
      return result;
    },
    [=](join_atom, strings_atom) {
      auto& stg = self->state.stage;
      CAF_MESSAGE("received 'join' request for strings");
      auto result = stg->add_unchecked_outbound_path<string>();
      stg->out().assign<string_downstream_manager>(result);
      return result;
    },
    [=](const stream<int32_t>& in) {
      CAF_MESSAGE("received handshake for integers");
      return self->state.stage->add_unchecked_inbound_path(in);
    },
    [=](const stream<string>& in) {
      CAF_MESSAGE("received handshake for strings");
      return self->state.stage->add_unchecked_inbound_path(in);
    }
  };
}

struct config : actor_system_config {
  config() {
    add_message_type<std::deque<std::string>>("deque<string>");
  }
};

using fixture = test_coordinator_fixture<>;

} // namespace <anonymous>

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(fused_streaming_tests, fixture)

CAF_TEST(depth_3_pipeline_with_fork) {
  auto src1 = sys.spawn(int_file_reader, 50u);
  auto src2 = sys.spawn(string_file_reader, 50u);
  auto stg = sys.spawn(stream_multiplexer);
  auto snk1 = sys.spawn(sum_up);
  auto snk2 = sys.spawn(collect);
  auto& st = deref<stream_multiplexer_actor>(stg).state;
  CAF_MESSAGE("connect sinks to the fused stage");
  self->send(snk1, join_atom::value, stg);
  self->send(snk2, join_atom::value, stg);
  sched.run();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 2u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 0u);
  CAF_MESSAGE("connect sources to the fused stage");
  self->send(stg * src1, "numbers.txt");
  self->send(stg * src2, "strings.txt");
  sched.run();
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 2u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 2u);
  run_until([&] {
    return st.stage->inbound_paths().empty() && st.stage->out().clean();
  });
  CAF_CHECK_EQUAL(st.stage->out().num_paths(), 2u);
  CAF_CHECK_EQUAL(st.stage->inbound_paths().size(), 0u);
  CAF_CHECK_EQUAL(deref<sum_up_actor>(snk1).state.x, 1275);
  CAF_CHECK_EQUAL(deref<collect_actor>(snk2).state.strings.size(), 50u);
  self->send_exit(stg, exit_reason::kill);
}

CAF_TEST_FIXTURE_SCOPE_END()
