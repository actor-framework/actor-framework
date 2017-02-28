/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include "caf/config.hpp"

#define CAF_SUITE stream_multiplexing
#include "caf/test/dsl.hpp"

#include <string>

#include "caf/all.hpp"
#include "caf/raw_event_based_actor.hpp"
#include "caf/forwarding_actor_proxy.hpp"

#include "caf/detail/incoming_stream_multiplexer.hpp"
#include "caf/detail/outgoing_stream_multiplexer.hpp"

using namespace caf;

namespace {

behavior dummy_basp(event_based_actor*) {
  return {
    [=](forward_atom, strong_actor_ptr& src,
        std::vector<strong_actor_ptr>& fwd_stack, strong_actor_ptr& dest,
        message_id mid, message& msg) {
      CAF_REQUIRE(dest != nullptr);
      dest->enqueue(make_mailbox_element(std::move(src), mid,
                                         std::move(fwd_stack), std::move(msg)),
                    nullptr);
    }
  };
}

void streamer_impl(event_based_actor* self, const actor& dest) {
  using buf = std::deque<int>;
  self->new_stream(
    // destination of the stream
    dest,
    // initialize state
    [](buf& xs) {
      xs = buf{1, 2, 3, 4, 5, 6, 7, 8, 9};
    },
    // get next element
    [](buf& xs, downstream<int>& out, size_t num) {
      auto n = std::min(num, xs.size());
      for (size_t i = 0; i < n; ++i)
        out.push(xs[i]);
      xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
    },
    // check whether we reached the end
    [](const buf& xs) {
      return xs.empty();
    },
    // handle result of the stream
    [](expected<int>) {
      // nop
    }
  );
}

behavior sum_up_impl(event_based_actor* self) {
  return {
    [=](stream<int>& in) {
      return self->add_sink(
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

struct fixture;

class stream_serv_class : public raw_event_based_actor,
                          public detail::stream_multiplexer::backend {
public:
  stream_serv_class(actor_config& cfg, actor basp, fixture& parent)
      : raw_event_based_actor(cfg),
        detail::stream_multiplexer::backend(std::move(basp)),
        fixture_(parent),
        incoming_(this, *this),
        outgoing_(this, *this) {
    // nop
  }

  behavior make_behavior() override {
    return {
      [=](stream_msg& x) -> delegated<message> {
        // Dispatching depends on the direction of the message.
        if (outgoing_.has_stream(x.sid)) {
          outgoing_(x);
        } else {
          incoming_(x);
        }
        return {};
      },
      [=](sys_atom, stream_msg& x) -> delegated<message> {
        // Stream message received from a proxy, always results in a new
        // stream from a local actor to a remote node.
        CAF_REQUIRE(holds_alternative<stream_msg::open>(x.content));
        outgoing_(x);
        return {};
      },
      [=](sys_atom, ok_atom, int32_t credit) {
        CAF_ASSERT(current_mailbox_element() != nullptr);
        auto cme = current_mailbox_element();
        if (cme->sender != nullptr) {
          auto& nid = cme->sender->node();
          add_credit(nid, credit);
        } else {
          CAF_LOG_ERROR("Received credit from an anonmyous stream server.");
        }
      },
      [=](exit_msg& x) {
        quit(x.reason);
      }
    };
  }

  void on_exit() override {
    CAF_CHECK_EQUAL(incoming_.num_streams(), 0u);
    CAF_CHECK_EQUAL(outgoing_.num_streams(), 0u);
    CAF_CHECK(streams().empty());
    remotes().clear();
  }

  strong_actor_ptr remote_stream_serv(const node_id& nid) override;

private:
  fixture& fixture_;
  detail::incoming_stream_multiplexer incoming_;
  detail::outgoing_stream_multiplexer outgoing_;
};

// Simulates a regular forwarding_actor_proxy by pushing a handle to the
// original to the forwarding stack and redirecting each message to the
// stream_serv.
class pseudo_proxy : public raw_event_based_actor {
public:
  pseudo_proxy(actor_config& cfg, actor stream_serv, actor original)
      : raw_event_based_actor(cfg),
        stream_serv_(std::move(stream_serv)),
        original_(std::move(original)){
    // nop
  }

  void enqueue(mailbox_element_ptr x, execution_unit* context) override {
    x->stages.emplace_back(actor_cast<strong_actor_ptr>(original_));
    auto msg = x->move_content_to_message();
    auto prefix = make_message(sys_atom::value);
    stream_serv_->enqueue(make_mailbox_element(std::move(x->sender), x->mid,
                                               std::move(x->stages),
                                               prefix + msg),
                          context);
  }

private:
  actor stream_serv_;
  actor original_;
};

struct fixture : test_coordinator_fixture<> {
  actor basp;
  actor streamer;
  actor sum_up;
  actor sum_up_proxy;
  actor stream_serv1;
  actor stream_serv2;

  fixture() {
    basp = sys.spawn(dummy_basp);
    sum_up = sys.spawn(sum_up_impl);
    stream_serv1 = sys.spawn<stream_serv_class>(basp, *this);
    stream_serv2 = sys.spawn<stream_serv_class>(basp, *this);
    sum_up_proxy = sys.spawn<pseudo_proxy>(stream_serv1, sum_up);
    CAF_MESSAGE("basp: " << to_string(basp));
    CAF_MESSAGE("sum_up: " << to_string(sum_up));
    CAF_MESSAGE("stream_serv: " << to_string(stream_serv1));
    sched.run();
  }

  ~fixture() {
    kill_em_all();
  }

  void kill_em_all() {
    for (auto x : {basp, streamer, sum_up, stream_serv1, stream_serv2})
      anon_send_exit(x, exit_reason::kill);
    sched.run();
  }
};

strong_actor_ptr stream_serv_class::remote_stream_serv(const node_id&) {
  auto res = strong_actor_ptr{ctrl()} == fixture_.stream_serv1
           ? fixture_.stream_serv2
           : fixture_.stream_serv1;
  return actor_cast<strong_actor_ptr>(res);
  // actor_cast<strong_actor_ptr>(fixture_.sum_up);
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(outgoing_stream_multiplexer_tests, fixture)

#define expect_network_traffic(source, destination)                            \
  expect((atom_value, strong_actor_ptr, std::vector<strong_actor_ptr>,         \
          strong_actor_ptr, message_id, message),                              \
         from(source).to(basp).with(forward_atom::value, source, _,            \
                                    destination, message_id::make(), _));

// Our first setup simply checks whether outgoing_stream_multiplexer intercepts
// stream handshakes. For this to happen, the forwarding actor proxy
// sum_up_proxy needs to re-write the initial `stream_msg::open`. It pushes
// "itself" (the actor it represents) onto the forwarding stack and redirects
// the handshake to the stream_serv (self). outgoing_stream_multiplexer then
// sends an ACK to the previous stage and a new OPEN to the remote stream_serv
// (which is missing in this simple setup).
CAF_TEST(stream_interception) {
  streamer = sys.spawn(streamer_impl, sum_up_proxy);
  sched.run_once();
  // streamer --('sys' stream_msg::open)--> stream_serv1
  expect((atom_value, stream_msg),
         from(streamer).to(stream_serv1).with(sys_atom::value, _));
  // streamer [via stream_serv1 / BASP] --(stream_msg::open)--> stream_serv2
  expect_network_traffic(streamer, stream_serv2);
  expect((stream_msg::open),
         from(_).to(stream_serv2).with(_, stream_serv1, _, _, _, false));
  // stream_serv2 [via BASP] --('sys', 'ok', 5)--> stream_serv1
  expect_network_traffic(stream_serv2, stream_serv1);
  expect((atom_value, atom_value, int32_t),
         from(stream_serv2).to(stream_serv1)
         .with(sys_atom::value, ok_atom::value, 5));
  // stream_serv2 --(stream_msg::open)--> sum_up
  expect((stream_msg::open),
         from(_).to(sum_up).with(_, stream_serv2, _, _, _, false));
  // sum_up --(stream_msg::ack_open)--> stream_serv2
  expect((stream_msg::ack_open),
         from(sum_up).to(stream_serv2).with(_, 5, _, false));
  // stream_serv2 [via BASP] --(stream_msg::ack_open)--> stream_serv1
  expect_network_traffic(stream_serv2, stream_serv1);
  expect((stream_msg::ack_open),
         from(stream_serv2).to(stream_serv1).with(_, 5, _, false));
  // stream_serv1 --('sys', 'ok', 5)--> stream_serv2
  expect_network_traffic(stream_serv1, stream_serv2);
  expect((atom_value, atom_value, int32_t),
         from(stream_serv1).to(stream_serv2)
         .with(sys_atom::value, ok_atom::value, 5));
  // stream_serv1 --(stream_msg::ack_open)--> streamer
  expect((stream_msg::ack_open),
         from(stream_serv1).to(streamer).with(_, 5, _, false));
  // streamer --(stream_msg::batch)--> stream_serv1
  expect((stream_msg::batch), from(streamer).to(stream_serv1).with(5, _, 0));
  // stream_serv1 [via BASP] --(stream_msg::batch)--> stream_serv2
  expect_network_traffic(stream_serv1, stream_serv2);
  expect((stream_msg::batch),
         from(stream_serv1).to(stream_serv2).with(5, _, 0));
  // stream_serv1 --(stream_msg::batch)--> sum_up
  expect((stream_msg::batch),
         from(stream_serv2).to(sum_up).with(5, _, 0));
  // sum_up --(stream_msg::ack_batch)--> stream_serv1
  expect((stream_msg::ack_batch),
         from(sum_up).to(stream_serv2).with(5, 0));
  // stream_serv2 [via BASP] --(stream_msg::ack_batch)--> stream_serv1
  expect_network_traffic(stream_serv2, stream_serv1);
  expect((stream_msg::ack_batch),
         from(stream_serv2).to(stream_serv1).with(5, 0));
  // stream_serv1 --(stream_msg::ack_batch)--> streamer
  expect((stream_msg::ack_batch),
         from(stream_serv1).to(streamer).with(5, 0));
  // streamer --(stream_msg::batch)--> stream_serv1
  expect((stream_msg::batch), from(streamer).to(stream_serv1).with(4, _, 1));
  // stream_serv1 [via BASP] --(stream_msg::batch)--> stream_serv2
  expect_network_traffic(stream_serv1, stream_serv2);
  expect((stream_msg::batch),
         from(stream_serv1).to(stream_serv2).with(4, _, 1));
  // stream_serv1 --(stream_msg::batch)--> sum_up
  expect((stream_msg::batch),
         from(stream_serv2).to(sum_up).with(4, _, 1));
  // sum_up --(stream_msg::ack_batch)--> stream_serv1
  expect((stream_msg::ack_batch),
         from(sum_up).to(stream_serv2).with(4, 1));
  // stream_serv2 [via BASP] --(stream_msg::ack_batch)--> stream_serv1
  expect_network_traffic(stream_serv2, stream_serv1);
  expect((stream_msg::ack_batch),
         from(stream_serv2).to(stream_serv1).with(4, 1));
  // stream_serv1 --(stream_msg::ack_batch)--> streamer
  expect((stream_msg::ack_batch),
         from(stream_serv1).to(streamer).with(4, 1));
  // streamer --(stream_msg::close)--> stream_serv1
  expect((stream_msg::close), from(streamer).to(stream_serv1).with());
  // stream_serv1 [via BASP] --(stream_msg::close)--> stream_serv2
  expect_network_traffic(stream_serv1, stream_serv2);
  expect((stream_msg::close), from(stream_serv1).to(stream_serv2).with());
  // stream_serv1 --(stream_msg::close)--> sum_up
  expect((stream_msg::close), from(stream_serv2).to(sum_up).with());
}

CAF_TEST_FIXTURE_SCOPE_END()

