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

#define CAF_SUITE high_level_streaming
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

using result_atom = atom_constant<atom("result")>;

behavior source(event_based_actor* self, size_t buf_size, bool do_cleanup) {
  using buf = std::deque<int>;
  return {
    [=](join_atom) {
      return self->make_source(
        // initialize state
        [=](buf& xs) {
          xs.resize(buf_size);
          std::iota(begin(xs), end(xs), 1);
        },
        // get next element
        [=](buf& xs, downstream<int>& out, size_t num) {
          auto n = std::min(num, xs.size());
          for (size_t i = 0; i < n; ++i) {
            out.push(xs[i]);
          }
          xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
        },
        // check whether we reached the end
        [=](const buf& xs) {
          if (xs.empty())
            return true;
          return false;
        },
        // cleanup
        [=](buf&, const error&) {
          if (do_cleanup)
            self->quit();
        }
      );
    }
  };
}

behavior sum_up(event_based_actor* self, actor buddy) {
  return {
    [=](stream<int>& in) {
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [](int& sum) {
          sum = 0; 
        },
        // processing step
        [=](int& sum, int x) {
          sum += x;
        },
        // cleanup and produce result message
        [=](int& sum) {
          self->send(buddy, result_atom::value, sum);
        }
      );
    }
  };
}

struct fixture {
  actor_system_config cfg;
  actor_system system;
  scoped_actor self;

  fixture()
      : system{cfg}
      , self{system} {
    // nop
  }

  int calc_sum_up_result(size_t n) {
    return n * (n + 1) / 2;
  }  

  ~fixture() {
    // nop
  }
};

} // namespace <anonymous>

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(high_level_streaming_tests, fixture)

CAF_TEST(setup_check) {
  int n = 10;
  auto src = system.spawn(source, n, false);
  auto snk = system.spawn(sum_up, self);
  self->send(snk * src, join_atom::value);
  self->receive([&](result_atom, int res){
    CAF_CHECK_EQUAL(res, calc_sum_up_result(n));
  });
}

CAF_TEST(call_quit_in_source_cleanup) {
  int n = 10;
  auto src = system.spawn(source, n, true);
  auto snk = system.spawn(sum_up, self);
  self->send(snk * src, join_atom::value);
  self->receive([&](result_atom, int res){
    CAF_CHECK_EQUAL(res, calc_sum_up_result(n));
  });
}

CAF_TEST(source_monitoring) {
  int n = 10;
  auto src = system.spawn(source, n, true);
  auto snk = system.spawn(sum_up, self);
  self->monitor(src);
  self->send(snk * src, join_atom::value);
  self->receive([&](down_msg&){
    CAF_CHECK(true);
  });
}

CAF_TEST(empty_source) {
  int n = 0;
  auto src = system.spawn(source, n, false);
  auto snk = system.spawn(sum_up, self);
  self->send(snk * src, join_atom::value);
  self->receive([&](result_atom, int res){
    CAF_CHECK_EQUAL(res, calc_sum_up_result(n));
  });
}
CAF_TEST_FIXTURE_SCOPE_END()
