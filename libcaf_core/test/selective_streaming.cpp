// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE selective_streaming

#include "core-test.hpp"

#include <memory>
#include <numeric>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/attach_continuous_stream_stage.hpp"
#include "caf/attach_stream_sink.hpp"
#include "caf/attach_stream_source.hpp"
#include "caf/broadcast_downstream_manager.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/stateful_actor.hpp"

using namespace caf;

namespace {

using value_type = std::pair<level, std::string>;

struct select {
  static bool apply(level x, const value_type& y) noexcept {
    return x == level::all || x == y.first;
  }
  bool operator()(level x, const value_type& y) const noexcept {
    return apply(x, y);
  }
};

using manager_type = broadcast_downstream_manager<value_type, level, select>;

using buf = std::vector<value_type>;

buf make_log(level lvl) {
  buf result{{level::trace, "trace1"},
             {level::trace, "trace2"},
             {level::debug, "debug1"},
             {level::error, "error1"},
             {level::trace, "trace3"}};
  auto predicate = [=](const value_type& x) { return !select::apply(lvl, x); };
  auto e = result.end();
  auto i = std::remove_if(result.begin(), e, predicate);
  if (i != e)
    result.erase(i, e);
  return result;
}

TESTEE_SETUP();

TESTEE(log_producer) {
  return {
    [=](level lvl) -> result<stream<value_type>> {
      auto res = attach_stream_source(
        self,
        // initialize state
        [=](buf& xs) { xs = make_log(lvl); },
        // get next element
        [](buf& xs, downstream<value_type>& out, size_t num) {
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
        },
        unit, policy::arg<manager_type>::value);
      auto& out = res.ptr()->out();
      static_assert(std::is_same<decltype(out), manager_type&>::value,
                    "source has wrong manager_type type");
      out.set_filter(res.outbound_slot(), lvl);
      return res;
    },
  };
}

TESTEE_STATE(log_dispatcher) {
  stream_stage_ptr<value_type, manager_type> stage;
};

TESTEE(log_dispatcher) {
  self->state.stage = attach_continuous_stream_stage(
    self,
    // initialize state
    [](unit_t&) {
      // nop
    },
    // processing step
    [](unit_t&, downstream<value_type>& out, value_type x) {
      out.push(std::move(x));
    },
    // cleanup
    [=](unit_t&, const error&) { CAF_MESSAGE(self->name() << " is done"); },
    policy::arg<manager_type>::value);
  return {
    [=](join_atom, level lvl) {
      auto& stg = self->state.stage;
      CAF_MESSAGE("received 'join' request");
      auto result = stg->add_outbound_path();
      stg->out().set_filter(result, lvl);
      return result;
    },
    [=](const stream<value_type>& in) {
      self->state.stage->add_inbound_path(in);
    },
  };
}

TESTEE_STATE(log_consumer) {
  std::vector<value_type> log;
};

TESTEE(log_consumer) {
  return {
    [=](stream<value_type>& in) {
      return attach_stream_sink(
        self,
        // input stream
        in,
        // initialize state
        [=](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, value_type x) {
          self->state.log.emplace_back(std::move(x));
        },
        // cleanup and produce result message
        [=](unit_t&, const error&) {
          CAF_MESSAGE(self->name() << " is done");
        });
    },
  };
}

} // namespace

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(selective_streaming_tests, test_coordinator_fixture<>)

CAF_TEST(select_all) {
  auto src = sys.spawn(log_producer);
  auto snk = sys.spawn(log_consumer);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, level::all);
  run();
  CAF_CHECK_EQUAL(deref<log_consumer_actor>(snk).state.log,
                  make_log(level::all));
}

CAF_TEST(select_trace) {
  auto src = sys.spawn(log_producer);
  auto snk = sys.spawn(log_consumer);
  CAF_MESSAGE(CAF_ARG(self) << CAF_ARG(src) << CAF_ARG(snk));
  CAF_MESSAGE("initiate stream handshake");
  self->send(snk * src, level::trace);
  run();
  CAF_CHECK_EQUAL(deref<log_consumer_actor>(snk).state.log,
                  make_log(level::trace));
}

CAF_TEST(forking) {
  auto src = sys.spawn(log_producer);
  auto stg = sys.spawn(log_dispatcher);
  auto snk1 = sys.spawn(log_consumer);
  auto snk2 = sys.spawn(log_consumer);
  sched.run();
  self->send(stg * src, level::all);
  self->send(snk1 * stg, join_atom_v, level::trace);
  self->send(snk2 * stg, join_atom_v, level::error);
  sched.run();
  auto& st = deref<log_dispatcher_actor>(stg).state;
  run_until([&] {
    return st.stage->inbound_paths().empty() && st.stage->out().clean();
  });
  CAF_CHECK_EQUAL(deref<log_consumer_actor>(snk1).state.log,
                  make_log(level::trace));
  CAF_CHECK_EQUAL(deref<log_consumer_actor>(snk2).state.log,
                  make_log(level::error));
  self->send(stg, exit_reason::kill);
}

CAF_TEST_FIXTURE_SCOPE_END()
