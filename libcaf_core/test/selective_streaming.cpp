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

#define CAF_SUITE selective_streaming

#include "caf/test/dsl.hpp"

#include <memory>
#include <numeric>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/atom.hpp"
#include "caf/broadcast_downstream_manager.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/stateful_actor.hpp"

using std::string;

using namespace caf;

namespace {

enum class level {
  all,
  trace,
  debug,
  warning,
  error
};

using value_type = std::pair<level, string>;

struct select {
  static bool apply(level x, const value_type& y) noexcept {
    return x == level::all || x == y.first;
  }
  bool operator()(level x, const value_type& y) const noexcept {
    return apply(x, y);
  }
};

using downstream_manager = broadcast_downstream_manager<value_type, level, select>;

using buf = std::vector<value_type>;

buf make_log(level lvl) {
  buf result{{level::trace, "trace1"},
             {level::trace, "trace2"},
             {level::debug, "debug1"},
             {level::error, "errro1"},
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
    [=](level lvl) -> output_stream<value_type> {
      auto res = self->make_source(
        // initialize state
        [=](buf& xs) {
          xs = make_log(lvl);
        },
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
        unit,
        policy::arg<downstream_manager>::value
      );
      auto& out = res.ptr()->out();
      static_assert(std::is_same<decltype(out), downstream_manager&>::value,
                    "source has wrong downstream_manager type");
      out.set_filter(res.outbound_slot(), lvl);
      return res;
    }
  };
}

TESTEE_STATE(log_dispatcher) {
  stream_stage_ptr<value_type, downstream_manager> stage;
};

TESTEE(log_dispatcher) {
  self->state.stage = self->make_continuous_stage(
    // initialize state
    [](unit_t&) {
      // nop
    },
    // processing step
    [](unit_t&, downstream<value_type>& out, value_type x) {
      out.push(std::move(x));
    },
    // cleanup
    [=](unit_t&, const error&) {
      CAF_MESSAGE(self->name() << " is done");
    },
    policy::arg<downstream_manager>::value
  );
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
    }
  };
}

TESTEE_STATE(log_consumer) {
  std::vector<value_type> log;
};

TESTEE(log_consumer) {
  return {
    [=](stream<value_type>& in) {
      return self->make_sink(
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
        }
      );
    }
  };
}

using fixture = test_coordinator_fixture<>;

} // namespace <anonymous>

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(selective_streaming_tests, fixture)

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
  self->send(snk1 * stg, join_atom::value, level::trace);
  self->send(snk2 * stg, join_atom::value, level::error);
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
