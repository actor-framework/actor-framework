/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

// this test is essentially a subset of streaming.cpp and tests whether the
// 3-stage pipeline compiles and runs when using a type-safe version of each
// stage

#include <deque>
#include <string>
#include <vector>

#define CAF_SUITE typed_stream_stages
#include "caf/test/dsl.hpp"

using std::string;

using namespace caf;

namespace {

using file_reader_actor = typed_actor<replies_to<string>
                                      ::with<stream<int>, string>>;

using filter_actor = typed_actor<replies_to<stream<int>, string>
                                 ::with<stream<int>, string>>;

using sum_up_actor = typed_actor<replies_to<stream<int>, string>::with<int>>;

file_reader_actor::behavior_type file_reader(file_reader_actor::pointer self) {
  using buf = std::deque<int>;
  return {
    [=](std::string& fname) {
      CAF_CHECK_EQUAL(fname, "test.txt");
      return self->make_source(
        // forward file name in handshake to next stage
        std::forward_as_tuple(std::move(fname)),
        // initialize state
        [=](buf& xs) {
          xs = buf{1, 2, 3, 4, 5, 6, 7, 8, 9};
        },
        // get next element
        [=](buf& xs, downstream<int>& out, size_t num) {
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

filter_actor::behavior_type filter(filter_actor::pointer self) {
  return {
    [=](stream<int>& in, std::string& fname) {
      CAF_CHECK_EQUAL(fname, "test.txt");
      return self->make_stage(
        // input stream
        in,
        // forward file name in handshake to next stage
        std::forward_as_tuple(std::move(fname)),
        // initialize state
        [=](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, downstream<int>& out, int x) {
          if ((x & 0x01) != 0)
            out.push(x);
        },
        // cleanup
        [=](unit_t&) {
          // nop
        }
      );
    }
  };
}

sum_up_actor::behavior_type sum_up(sum_up_actor::pointer self) {
  return {
    [=](stream<int>& in, std::string& fname) {
      CAF_CHECK_EQUAL(fname, "test.txt");
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

using fixture = test_coordinator_fixture<>;

using pipeline_actor = typed_actor<replies_to<string>::with<int>>;

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(typed_streaming_tests, fixture)

CAF_TEST(depth3_pipeline) {
  scoped_actor self{sys};
  auto source = sys.spawn(file_reader);
  auto stage = sys.spawn(filter);
  auto sink = sys.spawn(sum_up);
  auto pipeline = (sink * stage) * source;
  CAF_MESSAGE("source: " << to_string(source));
  CAF_MESSAGE("stage: " << to_string(stage));
  CAF_MESSAGE("sink: " << to_string(sink));
  CAF_MESSAGE("pipeline: " << to_string(pipeline));
  static_assert(std::is_same<decltype(pipeline), pipeline_actor>::value,
                "pipeline composition returned wrong type");
  sched.run();
  sched.after_next_enqueue([&] { sched.run(); });
  self->request(pipeline, infinite, "test.txt").receive(
    [&](int x) {
      CAF_CHECK_EQUAL(x, 25);
    },
    [&](error& err) {
      CAF_FAIL("error: " << sys.render(err));
    }
  );
}

CAF_TEST_FIXTURE_SCOPE_END()
