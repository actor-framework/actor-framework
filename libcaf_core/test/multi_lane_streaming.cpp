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

#include <set>
#include <map>
#include <string>
#include <numeric>
#include <fstream>
#include <iostream>
#include <iterator>
#include <unordered_set>

#define CAF_SUITE multi_lane_streaming
#include "caf/test/dsl.hpp"

#include "caf/random_topic_scatterer.hpp"

#include "caf/detail/pull5_gatherer.hpp"
#include "caf/detail/push5_scatterer.hpp"

using std::cout;
using std::endl;
using std::vector;
using std::string;

using namespace caf;

namespace {

using key_type = string;

using value_type = string;

using filter_type = std::vector<key_type>;

using element_type = std::pair<key_type, value_type>;

struct process_t {
  void operator()(unit_t&, downstream<element_type>& out, element_type x) {
    out.push(std::move(x));
  }
};

constexpr process_t process_fun = process_t{};

struct cleanup_t {
  void operator()(unit_t&) {
    // nop
  }
};

constexpr cleanup_t cleanup_fun = cleanup_t{};

struct selected_t {
  template <class T>
  bool operator()(const std::vector<string>& filter, const T& x) {
    for (auto& prefix : filter)
      if (get<0>(x).compare(0, prefix.size(), prefix.c_str()) == 0)
        return true;
    return false;
  }
};

struct stream_splitter_state {
  using stage_impl = stream_stage_impl<
    process_t, cleanup_t, random_gatherer,
    random_topic_scatterer<element_type, std::vector<key_type>, selected_t>>;
  intrusive_ptr<stage_impl> stage;
  static const char* name;
};

const char* stream_splitter_state::name = "stream_splitter";

behavior stream_splitter(stateful_actor<stream_splitter_state>* self) {
  stream_id id{self->ctrl(),
               self->new_request_id(message_priority::normal).integer_value()};
  using impl = stream_splitter_state::stage_impl;
  self->state.stage = make_counted<impl>(self, id, process_fun, cleanup_fun);
  self->state.stage->in().continuous(true);
  // Force the splitter to collect credit until reaching 3 in order
  // to receive only full batches from upstream (simplifies testing).
  // Restrict maximum credit per path to 5 (simplifies testing).
  self->state.stage->in().min_credit_assignment(3);
  self->state.stage->in().max_credit(5);
  self->streams().emplace(id, self->state.stage);
  return {
    [=](join_atom, filter_type filter) -> stream<element_type> {
      auto sid = self->streams().begin()->first;
      auto hdl = self->current_sender();
      if (!self->add_sink<element_type>(
            self->state.stage, sid, nullptr, hdl, no_stages, message_id::make(),
            stream_priority::normal, std::make_tuple()))
        return none;
      self->drop_current_message_id();
      self->state.stage->out().set_filter(sid, hdl, std::move(filter));
      return sid;
    },
    [=](const stream<element_type>& in) {
      auto& mgr = self->state.stage;
      if (!self->add_source(mgr, in.id(), none)) {
        CAF_FAIL("serve_as_stage failed");
      }
      self->streams().emplace(in.id(), mgr);
    }
  };
}

struct storage_state {
  static const char* name;
  std::vector<element_type> buf;
};

const char* storage_state::name = "storage";

behavior storage(stateful_actor<storage_state>* self,
                 actor source, filter_type filter) {
  self->send(self * source, join_atom::value, std::move(filter));
  return {
    [=](stream<element_type>& in) {
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, element_type x) {
          self->state.buf.emplace_back(std::move(x));
        },
        // cleanup and produce void "result"
        [](unit_t&) {
          CAF_LOG_INFO("storage done");
        },
        policy::arg<detail::pull5_gatherer, terminal_stream_scatterer>::value
      );
    },
    [=](get_atom) {
      return self->state.buf;
    }
  };
}

struct nores_streamer_state {
  static const char* name;
};

const char* nores_streamer_state::name = "nores_streamer";

void nores_streamer(stateful_actor<nores_streamer_state>* self,
                             const actor& dest) {
  CAF_LOG_INFO("nores_streamer initialized");
  using buf = std::deque<element_type>;
  self->make_source(
    // destination of the stream
    dest,
    // initialize state
    [&](buf& xs) {
      xs = buf{{"key1", "a"}, {"key2", "a"}, {"key1", "b"}, {"key2", "b"},
               {"key1", "c"}, {"key2", "c"}, {"key1", "d"}, {"key2", "d"}};
    },
    // get next element
    [=](buf& xs, downstream<element_type>& out, size_t num) {
      auto n = std::min(num, xs.size());
      for (size_t i = 0; i < n; ++i)
        out.push(xs[i]);
      xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
    },
    // check whether we reached the end
    [=](const buf& xs) {
      return xs.empty();
    },
    // handle result of the stream
    [=](expected<void>) {
      // nop
    });
}

struct config : actor_system_config {
public:
  config() {
    add_message_type<element_type>("element_type");
  }
};

using fixture = test_coordinator_fixture<config>;

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(multi_lane_streaming, fixture)

CAF_TEST(fork_setup) {
  using batch = std::vector<element_type>;
  auto splitter = sys.spawn(stream_splitter);
  sched.run();
  CAF_MESSAGE("spawn first sink");
  auto d1 = sys.spawn(storage, splitter, filter_type{"key1"});
  sched.run_once();
  expect((atom_value, filter_type),
         from(d1).to(splitter).with(join_atom::value, filter_type{"key1"}));
  expect((stream_msg::open),
         from(_).to(d1).with(_, splitter, _, _, _, false));
  expect((stream_msg::ack_open),
         from(d1).to(splitter).with(_, _, 5, _, false));
  CAF_MESSAGE("spawn second sink");
  auto d2 = sys.spawn(storage, splitter, filter_type{"key2"});
  sched.run_once();
  expect((atom_value, filter_type),
         from(d2).to(splitter).with(join_atom::value, filter_type{"key2"}));
  expect((stream_msg::open),
         from(_).to(d2).with(_, splitter, _, _, _, false));
  expect((stream_msg::ack_open),
         from(d2).to(splitter).with(_, _, 5, _, false));
  CAF_MESSAGE("spawn source");
  auto src = sys.spawn(nores_streamer, splitter);
  sched.run_once();
  // Handshake between src and splitter.
  expect((stream_msg::open),
         from(_).to(splitter).with(_, src, _, _, _, false));
  expect((stream_msg::ack_open),
         from(splitter).to(src).with(_, _, 5, _, false));
  // First batch.
  expect((stream_msg::batch),
         from(src).to(splitter)
         .with(5,
               batch{{"key1", "a"},
                     {"key2", "a"},
                     {"key1", "b"},
                     {"key2", "b"},
                     {"key1", "c"}},
               0));
  expect((stream_msg::batch),
         from(splitter).to(d2)
         .with(2, batch{{"key2", "a"}, {"key2", "b"}}, 0));
  expect((stream_msg::batch),
         from(splitter).to(d1)
         .with(3, batch{{"key1", "a"}, {"key1", "b"}, {"key1", "c"}}, 0));
  expect((stream_msg::ack_batch), from(d2).to(splitter).with(2, 0));
  expect((stream_msg::ack_batch), from(d1).to(splitter).with(3, 0));
  expect((stream_msg::ack_batch), from(splitter).to(src).with(5, 0));
  // Second batch.
  expect((stream_msg::batch),
         from(src).to(splitter)
         .with(3, batch{{"key2", "c"}, {"key1", "d"}, {"key2", "d"}}, 1));
  expect((stream_msg::batch),
         from(splitter).to(d1).with(1, batch{{"key1", "d"}}, 1));
  expect((stream_msg::batch),
         from(splitter).to(d2).with(2, batch{{"key2", "c"}, {"key2", "d"}}, 1));
  expect((stream_msg::ack_batch), from(d1).to(splitter).with(1, 1));
  expect((stream_msg::ack_batch), from(d2).to(splitter).with(2, 1));
  expect((stream_msg::ack_batch), from(splitter).to(src).with(3, 1));
  // Source is done, splitter remains open.
  expect((stream_msg::close), from(src).to(splitter).with());
  CAF_REQUIRE(!sched.has_job());
  CAF_MESSAGE("check content of storages");
  self->send(d1, get_atom::value);
  sched.run_once();
  self->receive(
    [](const batch& xs) {
      batch ys{{"key1", "a"}, {"key1", "b"}, {"key1", "c"}, {"key1", "d"}};
      CAF_REQUIRE_EQUAL(xs, ys);
    }
  );
  self->send(d2, get_atom::value);
  sched.run_once();
  self->receive(
    [](const batch& xs) {
      batch ys{{"key2", "a"}, {"key2", "b"}, {"key2", "c"}, {"key2", "d"}};
      CAF_REQUIRE_EQUAL(xs, ys);
    }
  );
  CAF_MESSAGE("spawn a second source");
  auto src2 = sys.spawn(nores_streamer, splitter);
  sched.run_once();
  // Handshake between src2 and splitter.
  expect((stream_msg::open),
         from(_).to(splitter).with(_, src2, _, _, _, false));
  expect((stream_msg::ack_open),
         from(splitter).to(src2).with(_, _, 5, _, false));
  // First batch.
  expect((stream_msg::batch),
         from(src2).to(splitter)
         .with(5,
               batch{{"key1", "a"},
                     {"key2", "a"},
                     {"key1", "b"},
                     {"key2", "b"},
                     {"key1", "c"}},
               0));
  expect((stream_msg::batch),
         from(splitter).to(d2)
         .with(2, batch{{"key2", "a"}, {"key2", "b"}}, 2));
  expect((stream_msg::batch),
         from(splitter).to(d1)
         .with(3, batch{{"key1", "a"}, {"key1", "b"}, {"key1", "c"}}, 2));
  expect((stream_msg::ack_batch), from(d2).to(splitter).with(2, 2));
  expect((stream_msg::ack_batch), from(d1).to(splitter).with(3, 2));
  expect((stream_msg::ack_batch), from(splitter).to(src2).with(5, 0));
  // Second batch.
  expect((stream_msg::batch),
         from(src2).to(splitter)
         .with(3, batch{{"key2", "c"}, {"key1", "d"}, {"key2", "d"}}, 1));
  expect((stream_msg::batch),
         from(splitter).to(d1).with(1, batch{{"key1", "d"}}, 3));
  expect((stream_msg::batch),
         from(splitter).to(d2).with(2, batch{{"key2", "c"}, {"key2", "d"}}, 3));
  expect((stream_msg::ack_batch), from(d1).to(splitter).with(1, 3));
  expect((stream_msg::ack_batch), from(d2).to(splitter).with(2, 3));
  expect((stream_msg::ack_batch), from(splitter).to(src2).with(3, 1));
  // Source is done, splitter remains open.
  expect((stream_msg::close), from(src2).to(splitter).with());
  CAF_REQUIRE(!sched.has_job());
  CAF_MESSAGE("check content of storages again");
  self->send(d1, get_atom::value);
  sched.run_once();
  self->receive(
    [](const batch& xs) {
      batch ys0{{"key1", "a"}, {"key1", "b"}, {"key1", "c"}, {"key1", "d"}};
      auto ys = ys0;
      ys.insert(ys.end(), ys0.begin(), ys0.end()); // all elements twice
      CAF_REQUIRE_EQUAL(xs, ys);
    }
  );
  self->send(d2, get_atom::value);
  sched.run_once();
  self->receive(
    [](const batch& xs) {
      batch ys0{{"key2", "a"}, {"key2", "b"}, {"key2", "c"}, {"key2", "d"}};
      auto ys = ys0;
      ys.insert(ys.end(), ys0.begin(), ys0.end()); // all elements twice
      CAF_REQUIRE_EQUAL(xs, ys);
    }
  );
  CAF_MESSAGE("shutdown");
  anon_send_exit(splitter, exit_reason::kill);
  sched.run();
}

CAF_TEST_FIXTURE_SCOPE_END()
