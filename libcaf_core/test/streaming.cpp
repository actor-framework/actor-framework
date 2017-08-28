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

#include <string>
#include <numeric>
#include <fstream>
#include <iostream>
#include <iterator>

#define CAF_SUITE streaming
#include "caf/test/dsl.hpp"

#include "caf/detail/pull5_gatherer.hpp"
#include "caf/detail/push5_scatterer.hpp"

using std::cout;
using std::endl;
using std::string;

using namespace caf;

namespace {

struct file_reader_state {
  static const char* name;
};

const char* file_reader_state::name = "file_reader";

behavior file_reader(stateful_actor<file_reader_state>* self) {
  using buf = std::deque<int>;
  return {
    [=](std::string& fname) -> stream<int> {
      CAF_CHECK_EQUAL(fname, "test.txt");
      return self->make_source(
        // forward file name in handshake to next stage
        std::forward_as_tuple(std::move(fname)),
        // initialize state
        [&](buf& xs) {
          xs = buf{1, 2, 3, 4, 5, 6, 7, 8, 9};
        },
        // get next element
        [=](buf& xs, downstream<int>& out, size_t num) {
          CAF_MESSAGE("push " << num << " more messages downstream");
          auto n = std::min(num, xs.size());
          for (size_t i = 0; i < n; ++i)
            out.push(xs[i]);
          xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
        },
        // check whether we reached the end
        [=](const buf& xs) {
          return xs.empty();
        },
        policy::arg<detail::push5_scatterer<int>>::value
      );
    }
  };
}

struct streamer_state {
  static const char* name;
};

const char* streamer_state::name = "streamer";

void streamer(stateful_actor<streamer_state>* self, const actor& dest) {
  using buf = std::deque<int>;
  self->make_source(
    // destination of the stream
    dest,
    // "file name" as seen by the next stage
    std::make_tuple("test.txt"),
    // initialize state
    [&](buf& xs) {
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
    },
    // handle result of the stream
    [=](expected<int>) {
      // nop
    },
    policy::arg<detail::push5_scatterer<int>>::value
  );
}

struct filter_state {
  static const char* name;
};

const char* filter_state::name = "filter";

behavior filter(stateful_actor<filter_state>* self) {
  return {
    [=](stream<int>& in, std::string& fname) -> stream<int> {
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
        },
        policy::arg<detail::pull5_gatherer, detail::push5_scatterer<int>>::value
      );
    }
  };
}

struct broken_filter_state {
  static const char* name;
};

const char* broken_filter_state::name = "broken_filter";

behavior broken_filter(stateful_actor<broken_filter_state>*) {
  return {
    [=](stream<int>& x, const std::string& fname) -> stream<int> {
      CAF_CHECK_EQUAL(fname, "test.txt");
      return x;
    }
  };
}

struct sum_up_state {
  static const char* name;
};

const char* sum_up_state::name = "sum_up";

behavior sum_up(stateful_actor<sum_up_state>* self) {
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
        },
        policy::arg<detail::pull5_gatherer, terminal_stream_scatterer>::value
      );
    }
  };
}

struct drop_all_state {
  static const char* name;
};

const char* drop_all_state::name = "drop_all";

behavior drop_all(stateful_actor<drop_all_state>* self) {
  return {
    [=](stream<int>& in, std::string& fname) {
      CAF_CHECK_EQUAL(fname, "test.txt");
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [](unit_t&, int) {
          // nop
        },
        // cleanup and produce void "result"
        [](unit_t&) {
          CAF_LOG_INFO("drop_all done");
        },
        policy::arg<detail::pull5_gatherer, terminal_stream_scatterer>::value
      );
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
  using buf = std::deque<int>;
  self->make_source(
    // destination of the stream
    dest,
    // "file name" for the next stage
    std::make_tuple("test.txt"),
    // initialize state
    [&](buf& xs) {
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
    },
    // handle result of the stream
    [=](expected<void>) {
      // nop
    },
    policy::arg<detail::push5_scatterer<int>>::value
  );
}

struct stream_multiplexer_state {
  stream_manager_ptr stage;
  static const char* name;
};

const char* stream_multiplexer_state::name = "stream_multiplexer";

behavior stream_multiplexer(stateful_actor<stream_multiplexer_state>* self) {
  { // extra scope for hiding state for initialization from the lambdas below
    auto process = [](unit_t&, downstream<int>& out, int x) {
      out.push(x);
    };
    auto cleanup = [](unit_t&) {
      // nop
    };
    auto sid = self->make_stream_id();
    using impl = stream_stage_impl<decltype(process), decltype(cleanup),
          detail::pull5_gatherer,
          detail::push5_scatterer<int>>;
    self->state.stage = make_counted<impl>(self, sid, process, cleanup);
    self->state.stage->in().continuous(true);
    self->streams().emplace(sid, self->state.stage);
  }
  return {
    [=](join_atom) -> stream<int> {
      CAF_MESSAGE("received 'join' request");
      auto sid = self->streams().begin()->first;
      if (!self->add_sink<int>(
            self->state.stage, sid, nullptr, self->current_sender(), no_stages,
            self->current_message_id(), stream_priority::normal,
            std::make_tuple<std::string>("test.txt"))) {
        auto rp = self->make_response_promise();
        rp.deliver(sec::invalid_stream_state);
        return none;
      }
      self->drop_current_message_id();
      return sid;
    },
    [=](const stream<int>& in, std::string& fname) {
      CAF_CHECK_EQUAL(fname, "test.txt");
      auto& mgr = self->state.stage;
      if (!self->add_source(mgr, in.id(), none)) {
        CAF_FAIL("serve_as_stage failed");
      }
      self->streams().emplace(in.id(), mgr);
    },
  };
}

using fixture = test_coordinator_fixture<>;

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(local_streaming_tests, fixture)

CAF_TEST(no_downstream) {
  CAF_MESSAGE("opening streams must fail if no downstream stage exists");
  auto source = sys.spawn(file_reader);
  self->send(source, "test.txt");
  sched.run();
  CAF_CHECK_EQUAL(fetch_result(), sec::no_downstream_stages_defined);
  CAF_CHECK(deref(source).streams().empty());
}

CAF_TEST(broken_pipeline) {
  CAF_MESSAGE("streams must abort if a stage fails to initialize its state");
  auto source = sys.spawn(file_reader);
  auto stage = sys.spawn(broken_filter);
  auto pipeline = stage * source;
  sched.run();
  // self --("test.txt")--> source
  self->send(pipeline, "test.txt");
  expect((std::string), from(self).to(source).with("test.txt"));
  // source --(stream_msg::open)--> stage
  expect((stream_msg::open),
         from(self).to(stage).with(_, source, _, _, _, _, false));
  CAF_CHECK(!deref(source).streams().empty());
  CAF_CHECK(deref(stage).streams().empty());
  // stage --(stream_msg::forced_drop)--> source
  expect((stream_msg::forced_drop),
         from(stage).to(source).with(sec::stream_init_failed));
  CAF_CHECK(deref(source).streams().empty());
  CAF_CHECK(deref(stage).streams().empty());
  CAF_CHECK_EQUAL(fetch_result(), sec::stream_init_failed);
}

CAF_TEST(incomplete_pipeline) {
  CAF_MESSAGE("streams must abort if not reaching a sink");
  auto source = sys.spawn(file_reader);
  auto stage = sys.spawn(filter);
  auto pipeline = stage * source;
  sched.run();
  // self --("test.txt")--> source
  self->send(pipeline, "test.txt");
  expect((std::string), from(self).to(source).with("test.txt"));
  // source --(stream_msg::open)--> stage
  CAF_REQUIRE(sched.prioritize(stage));
  expect((stream_msg::open),
         from(self).to(stage).with(_, source, _, _,  _, _, false));
  CAF_CHECK(!deref(source).streams().empty());
  CAF_CHECK(deref(stage).streams().empty());
  // stage --(stream_msg::forced_drop)--> source
  expect((stream_msg::forced_drop),
         from(stage).to(source).with(sec::stream_init_failed));
  CAF_CHECK(deref(source).streams().empty());
  CAF_CHECK(deref(stage).streams().empty());
  CAF_CHECK_EQUAL(fetch_result(), sec::no_downstream_stages_defined);
}

CAF_TEST(depth2_pipeline) {
  auto source = sys.spawn(file_reader);
  auto sink = sys.spawn(sum_up);
  auto pipeline = sink * source;
  // run initialization code
  sched.run();
  // self -------("test.txt")-------> source
  self->send(pipeline, "test.txt");
  expect((std::string), from(self).to(source).with("test.txt"));
  CAF_CHECK(!deref(source).streams().empty());
  CAF_CHECK(deref(sink).streams().empty());
  // source ----(stream_msg::open)----> sink
  expect((stream_msg::open),
         from(self).to(sink).with(_, source, _, _, _, _, false));
  // source <----(stream_msg::ack_open)------ sink
  expect((stream_msg::ack_open), from(sink).to(source).with(_, _, 5, _, false));
  // source ----(stream_msg::batch)---> sink
  expect((stream_msg::batch),
         from(source).to(sink).with(5, std::vector<int>{1, 2, 3, 4, 5}, 0));
  // source <--(stream_msg::ack_batch)---- sink
  expect((stream_msg::ack_batch), from(sink).to(source).with(5, 0));
  // source ----(stream_msg::batch)---> sink
  expect((stream_msg::batch),
         from(source).to(sink).with(4, std::vector<int>{6, 7, 8, 9}, 1));
  // source <--(stream_msg::ack_batch)---- sink
  expect((stream_msg::ack_batch), from(sink).to(source).with(4, 1));
  // source ----(stream_msg::close)---> sink
  expect((stream_msg::close), from(source).to(sink).with());
  CAF_CHECK(deref(source).streams().empty());
  CAF_CHECK(deref(sink).streams().empty());
}

CAF_TEST(depth3_pipeline_order1) {
  // Order 1 is an idealized flow where batch messages travel from the source
  // to the sink and then ack_batch messages travel backwards, starting the
  // process over again.
  CAF_MESSAGE("check fully initialized pipeline with event order 1");
  auto source = sys.spawn(file_reader);
  auto stage = sys.spawn(filter);
  auto sink = sys.spawn(sum_up);
  auto pipeline = self * sink * stage * source;
  // run initialization code
  sched.run();
  // self --("test.txt")--> source
  CAF_CHECK(self->mailbox().empty());
  self->send(pipeline, "test.txt");
  expect((std::string), from(self).to(source).with("test.txt"));
  CAF_CHECK(self->mailbox().empty());
  CAF_CHECK(!deref(source).streams().empty());
  CAF_CHECK(deref(stage).streams().empty());
  CAF_CHECK(deref(sink).streams().empty());
  // source --(stream_msg::open)--> stage
  expect((stream_msg::open),
         from(self).to(stage).with(_, source, _, _, _, false));
  CAF_CHECK(!deref(source).streams().empty());
  CAF_CHECK(!deref(stage).streams().empty());
  CAF_CHECK(deref(sink).streams().empty());
  // stage --(stream_msg::open)--> sink
  expect((stream_msg::open),
         from(self).to(sink).with(_, stage, _, _, _, false));
  CAF_CHECK(!deref(source).streams().empty());
  CAF_CHECK(!deref(stage).streams().empty());
  CAF_CHECK(!deref(sink).streams().empty());
  // sink --(stream_msg::ack_open)--> stage
  expect((stream_msg::ack_open), from(sink).to(stage).with(_, _, 5, _, false));
  // stage --(stream_msg::ack_open)--> source
  expect((stream_msg::ack_open),
         from(stage).to(source).with(_, _, 5, _, false));
  // source --(stream_msg::batch)--> stage
  expect((stream_msg::batch),
         from(source).to(stage).with(5, std::vector<int>{1, 2, 3, 4, 5}, 0));
  // stage --(stream_msg::batch)--> sink
  expect((stream_msg::batch),
         from(stage).to(sink).with(3, std::vector<int>{1, 3, 5}, 0));
  // stage --(stream_msg::batch)--> source
  expect((stream_msg::ack_batch), from(stage).to(source).with(5, 0));
  // sink --(stream_msg::batch)--> stage
  expect((stream_msg::ack_batch), from(sink).to(stage).with(3, 0));
  // source --(stream_msg::batch)--> stage
  expect((stream_msg::batch),
         from(source).to(stage).with(4, std::vector<int>{6, 7, 8, 9}, 1));
  // stage --(stream_msg::batch)--> sink
  expect((stream_msg::batch),
         from(stage).to(sink).with(2, std::vector<int>{7, 9}, 1));
  // stage --(stream_msg::batch)--> source
  expect((stream_msg::ack_batch), from(stage).to(source).with(4, 1));
  // sink --(stream_msg::batch)--> stage
  expect((stream_msg::ack_batch), from(sink).to(stage).with(2, 1));
  // source ----(stream_msg::close)---> stage
  expect((stream_msg::close), from(source).to(stage).with());
  // stage ----(stream_msg::close)---> sink
  expect((stream_msg::close), from(stage).to(sink).with());
  // sink ----(result: 25)---> self
  expect((int), from(sink).to(self).with(25));
}

CAF_TEST(depth3_pipeline_order2) {
  // Order 2 assumes that source and stage communicate faster then the sink.
  // This means batches and acks go as fast as possible between source and
  // stage, only slowing down if an ack from the sink is needed to drive
  // computation forward.
  CAF_MESSAGE("check fully initialized pipeline with event order 2");
  auto source = sys.spawn(file_reader);
  auto stage = sys.spawn(filter);
  auto sink = sys.spawn(sum_up);
  CAF_MESSAGE("source: " << to_string(source));
  CAF_MESSAGE("stage: " << to_string(stage));
  CAF_MESSAGE("sink: " << to_string(sink));
  auto pipeline = self * sink * stage * source;
  // run initialization code
  sched.run();
  // self --("test.txt")--> source
  CAF_CHECK(self->mailbox().empty());
  self->send(pipeline, "test.txt");
  expect((std::string), from(self).to(source).with("test.txt"));
  // source --(stream_msg::open)--> stage
  expect((stream_msg::open),
         from(self).to(stage).with(_, source, _, _, _, false));
  // stage --(stream_msg::ack_open)--> source
  expect((stream_msg::ack_open),
         from(stage).to(source).with(_, _, 5, _, false));
  // source --(stream_msg::batch)--> stage
  expect((stream_msg::batch),
         from(source).to(stage).with(5, std::vector<int>{1, 2, 3, 4, 5}, 0));
  // stage --(stream_msg::ack_batch)--> source
  // The stage has filtered {2, 4}, which means {1, 3, 5} are now buffered at
  // the stage. New credit assigned to the source is 2, since there's no credit
  // to send data downstream and the buffer is only allowed to keep 5 elements
  // total.
  expect((stream_msg::ack_batch), from(stage).to(source).with(2, 0));
  // source --(stream_msg::batch)--> stage
  expect((stream_msg::batch),
         from(source).to(stage).with(2, std::vector<int>{6, 7}, 1));
  // stage --(stream_msg::ack_batch)--> source
  // The stage has filtered {6}, which means {1, 3, 5, 7} are now buffered at
  // the stage. New credit assigned to the source is hence 1.
  expect((stream_msg::ack_batch), from(stage).to(source).with(1, 1));
  // source --(stream_msg::batch)--> stage
  expect((stream_msg::batch),
         from(source).to(stage).with(1, std::vector<int>{8}, 2));
  // stage --(stream_msg::ack_batch)--> source
  // The stage has dropped 8, still leaving 1 space in the buffer.
  expect((stream_msg::ack_batch), from(stage).to(source).with(1, 2));
  // source --(stream_msg::batch)--> stage
  expect((stream_msg::batch),
         from(source).to(stage).with(1, std::vector<int>{9}, 3));
  // At this point, stage is not allowed to signal demand because it no longer
  // has any capacity in its buffer nor did it receive downstream demand yet.
  disallow((stream_msg::ack_batch), from(stage).to(source).with(_, _));
  // stage --(stream_msg::open)--> sink
  expect((stream_msg::open),
         from(self).to(sink).with(_, stage, _, _, _, false));
  // sink --(stream_msg::ack_open)--> stage (finally)
  expect((stream_msg::ack_open), from(sink).to(stage).with(_, _, 5, _, false));
  // stage --(stream_msg::ack_batch)--> source
  // The stage has now emptied its buffer and is able to grant more credit.
  expect((stream_msg::ack_batch), from(stage).to(source).with(5, 3));
  // source ----(stream_msg::close)---> stage
  // The source can now initiate shutting down the stream since it successfully
  // produced all elements.
  expect((stream_msg::close), from(source).to(stage).with());
  // stage --(stream_msg::batch)--> sink
  expect((stream_msg::batch),
         from(stage).to(sink).with(5, std::vector<int>{1, 3, 5, 7, 9}, 0));
  // sink --(stream_msg::ack_batch)--> stage
  expect((stream_msg::ack_batch), from(sink).to(stage).with(5, 0));
  // stage ----(stream_msg::close)---> sink
  expect((stream_msg::close), from(stage).to(sink).with());
  // sink ----(result: 25)---> self
  expect((int), from(sink).to(self).with(25));
}

CAF_TEST(broken_pipeline_stramer) {
  CAF_MESSAGE("streams must abort if a stage fails to initialize its state");
  auto stage = sys.spawn(broken_filter);
  // run initialization code
  sched.run();
  auto source = sys.spawn(streamer, stage);
  // run initialization code
  sched.run_once();
  // source --(stream_msg::open)--> stage
  expect((stream_msg::open),
         from(source).to(stage).with(_, source, _, _, _, false));
  CAF_CHECK(!deref(source).streams().empty());
  CAF_CHECK(deref(stage).streams().empty());
  // stage --(stream_msg::forced_drop)--> source
  expect((stream_msg::forced_drop),
         from(stage).to(source).with(sec::stream_init_failed));
  CAF_CHECK(deref(source).streams().empty());
  CAF_CHECK(deref(stage).streams().empty());
  // The stage failed during handshake. Thus, the source is still responsible
  // for sending an error message (to itself in this case).
  // source ----(error)---> source
  expect((error), from(source).to(source).with(_));
}

CAF_TEST(depth2_pipeline_streamer) {
  auto sink = sys.spawn(sum_up);
  // run initialization code
  sched.run();
  auto source = sys.spawn(streamer, sink);
  // run initialization code
  sched.run_once();
  // source ----(stream_msg::open)----> sink
  expect((stream_msg::open),
         from(source).to(sink).with(_, source, _, _, _, false));
  // source <----(stream_msg::ack_open)------ sink
  expect((stream_msg::ack_open), from(sink).to(source).with(_, _, 5, _, false));
  // source ----(stream_msg::batch)---> sink
  expect((stream_msg::batch),
         from(source).to(sink).with(5, std::vector<int>{1, 2, 3, 4, 5}, 0));
  // source <--(stream_msg::ack_batch)---- sink
  expect((stream_msg::ack_batch), from(sink).to(source).with(5, 0));
  // source ----(stream_msg::batch)---> sink
  expect((stream_msg::batch),
         from(source).to(sink).with(4, std::vector<int>{6, 7, 8, 9}, 1));
  // source <--(stream_msg::ack_batch)---- sink
  expect((stream_msg::ack_batch), from(sink).to(source).with(4, 1));
  // source ----(stream_msg::close)---> sink
  expect((stream_msg::close), from(source).to(sink).with());
  // sink ----(result: 25)---> source
  expect((int), from(sink).to(source).with(45));
}

CAF_TEST(stream_without_result) {
  auto sink = sys.spawn(drop_all);
  // run initialization code
  sched.run();
  auto source = sys.spawn(nores_streamer, sink);
  // run initialization code
  sched.run_once();
  // source ----(stream_msg::open)----> sink
  expect((stream_msg::open),
         from(source).to(sink).with(_, source, _, _, _, false));
  // source <----(stream_msg::ack_open)------ sink
  expect((stream_msg::ack_open), from(sink).to(source).with(_, _, 5, _, false));
  // source ----(stream_msg::batch)---> sink
  expect((stream_msg::batch),
         from(source).to(sink).with(5, std::vector<int>{1, 2, 3, 4, 5}, 0));
  // source <--(stream_msg::ack_batch)---- sink
  expect((stream_msg::ack_batch), from(sink).to(source).with(5, 0));
  // source ----(stream_msg::batch)---> sink
  expect((stream_msg::batch),
         from(source).to(sink).with(4, std::vector<int>{6, 7, 8, 9}, 1));
  // source <--(stream_msg::ack_batch)---- sink
  expect((stream_msg::ack_batch), from(sink).to(source).with(4, 1));
  // source ----(stream_msg::close)---> sink
  expect((stream_msg::close), from(source).to(sink).with());
  // sink ----(result: <empty>)---> source
  expect((void), from(sink).to(source).with());
}

CAF_TEST(multiplexed_pipeline) {
  auto multiplexer = sys.spawn(stream_multiplexer);
  auto joining_drop_all = [=](stateful_actor<drop_all_state>* self) {
    self->send(self * multiplexer, join_atom::value);
    return drop_all(self);
  };
  sched.run();
  CAF_MESSAGE("spawn first sink");
  auto d1 = sys.spawn(joining_drop_all);
  sched.run_once();
  expect((atom_value), from(d1).to(multiplexer).with(join_atom::value));
  expect((stream_msg::open),
         from(_).to(d1).with(_, multiplexer, _, _, _, false));
  expect((stream_msg::ack_open),
         from(d1).to(multiplexer).with(_, _, 5, _, false));
  CAF_MESSAGE("spawn second sink");
  auto d2 = sys.spawn(joining_drop_all);
  sched.run_once();
  expect((atom_value), from(d2).to(multiplexer).with(join_atom::value));
  expect((stream_msg::open),
         from(_).to(d2).with(_, multiplexer, _, _, _, false));
  expect((stream_msg::ack_open),
         from(d2).to(multiplexer).with(_, _, 5, _, false));
  CAF_MESSAGE("spawn source");
  auto src = sys.spawn(nores_streamer, multiplexer);
  sched.run_once();
  // Handshake between src and multiplexer.
  expect((stream_msg::open),
         from(_).to(multiplexer).with(_, src, _, _, _, false));
  expect((stream_msg::ack_open),
         from(multiplexer).to(src).with(_, _, 5, _, false));
  // First batch.
  expect((stream_msg::batch),
         from(src).to(multiplexer).with(5, std::vector<int>{1, 2, 3, 4, 5}, 0));
  expect((stream_msg::batch),
         from(multiplexer).to(d1).with(5, std::vector<int>{1, 2, 3, 4, 5}, 0));
  expect((stream_msg::batch),
         from(multiplexer).to(d2).with(5, std::vector<int>{1, 2, 3, 4, 5}, 0));
  expect((stream_msg::ack_batch), from(d1).to(multiplexer).with(5, 0));
  expect((stream_msg::ack_batch), from(d2).to(multiplexer).with(5, 0));
  expect((stream_msg::ack_batch), from(multiplexer).to(src).with(5, 0));
  // Second batch.
  expect((stream_msg::batch),
         from(src).to(multiplexer).with(4, std::vector<int>{6, 7, 8, 9}, 1));
  expect((stream_msg::batch),
         from(multiplexer).to(d1).with(4, std::vector<int>{6, 7, 8, 9}, 1));
  expect((stream_msg::batch),
         from(multiplexer).to(d2).with(4, std::vector<int>{6, 7, 8, 9}, 1));
  expect((stream_msg::ack_batch), from(d1).to(multiplexer).with(4, 1));
  expect((stream_msg::ack_batch), from(d2).to(multiplexer).with(4, 1));
  expect((stream_msg::ack_batch), from(multiplexer).to(src).with(4, 1));
  // Source is done, multiplexer remains open.
  expect((stream_msg::close), from(src).to(multiplexer).with());
  CAF_REQUIRE(!sched.has_job());
  CAF_MESSAGE("spawn a second source");
  auto src2 = sys.spawn(nores_streamer, multiplexer);
  sched.run_once();
  // Handshake between src2 and multiplexer.
  expect((stream_msg::open),
         from(_).to(multiplexer).with(_, src2, _, _, _, false));
  expect((stream_msg::ack_open),
       from(multiplexer).to(src2).with(_, _, 5, _, false));
  // First batch.
  expect((stream_msg::batch),
         from(src2).to(multiplexer).with(5, std::vector<int>{1, 2, 3, 4, 5}, 0));
  expect((stream_msg::batch),
         from(multiplexer).to(d1).with(5, std::vector<int>{1, 2, 3, 4, 5}, 2));
  expect((stream_msg::batch),
         from(multiplexer).to(d2).with(5, std::vector<int>{1, 2, 3, 4, 5}, 2));
  expect((stream_msg::ack_batch), from(d1).to(multiplexer).with(5, 2));
  expect((stream_msg::ack_batch), from(d2).to(multiplexer).with(5, 2));
  expect((stream_msg::ack_batch), from(multiplexer).to(src2).with(5, 0));
  // Second batch.
  expect((stream_msg::batch),
         from(src2).to(multiplexer).with(4, std::vector<int>{6, 7, 8, 9}, 1));
  expect((stream_msg::batch),
         from(multiplexer).to(d1).with(4, std::vector<int>{6, 7, 8, 9}, 3));
  expect((stream_msg::batch),
         from(multiplexer).to(d2).with(4, std::vector<int>{6, 7, 8, 9}, 3));
  expect((stream_msg::ack_batch), from(d1).to(multiplexer).with(4, 3));
  expect((stream_msg::ack_batch), from(d2).to(multiplexer).with(4, 3));
  expect((stream_msg::ack_batch), from(multiplexer).to(src2).with(4, 1));
  // Source is done, multiplexer remains open.
  expect((stream_msg::close), from(src2).to(multiplexer).with());
  CAF_REQUIRE(!sched.has_job());
  CAF_MESSAGE("shutdown");
  anon_send_exit(multiplexer, exit_reason::kill);
  sched.run();
}

CAF_TEST_FIXTURE_SCOPE_END()
