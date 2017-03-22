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

#include <set>
#include <map>
#include <string>
#include <numeric>
#include <fstream>
#include <iostream>
#include <iterator>
#include <unordered_set>

#define CAF_SUITE manual_stream_management
#include "caf/test/dsl.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/test_multiplexer.hpp"

using std::cout;
using std::endl;
using std::string;

using namespace caf;

namespace {

using peer_atom = atom_constant<atom("peer")>;

using topics = std::set<std::string>;

/// A peer is connected via two streams: one for inputs and one for outputs.
class peer_state : public ref_counted {
public:
  using handler_ptr = intrusive_ptr<stream_handler>;
  using buf_type = std::deque<int>;

  struct {
    stream<int> sid;
    handler_ptr ptr;
  }
  in;

  struct {
    stream<int> sid;
    handler_ptr ptr;
    buf_type buf;
    topics filter;
  }
  out;

  peer_state() {
    // nop
  }
};

struct state;

/// A policy used by the core actor to broadcast local messages to all remotes.
class core_broadcast_policy final : public downstream_policy {
public:
  core_broadcast_policy(state& st) : st_(st) {
    // nop
  }

  void push(abstract_downstream& out, size_t*) override;

  size_t available_credit(const abstract_downstream& out) override;

  static std::unique_ptr<downstream_policy> make(state& st) {
    return std::unique_ptr<downstream_policy>{new core_broadcast_policy(st)};
  }

private:
  /// State of the parent actor.
  state& st_;
};

struct state {
  using peer_ptr = intrusive_ptr<peer_state>;

  using stream_handler_ptr = intrusive_ptr<stream_handler>;

  state(event_based_actor* self_p) : self(self_p) {
    // nop
  }

  /// Returns the peer to the currently processed stream message, i.e.,
  /// `self->current_mailbox_element()->stages.back()` if the stages stack
  /// is not empty.
  strong_actor_ptr prev_peer_from_handshake() {
    auto& xs = self->current_mailbox_element()->content();
    strong_actor_ptr res;
    if (xs.match_elements<stream_msg>()) {
      auto& x = xs.get_as<stream_msg>(0);
      if (holds_alternative<stream_msg::open>(x.content)) {
        res = get<stream_msg::open>(x.content).prev_stage;
      }
    }
    return res;
    // auto& stages = self->current_mailbox_element()->stages;
    // return stages.empty() ? nullptr : stages.back();
  }

  template <class T>
  stream<int> source(const peer_ptr& ptr, const T& handshake_argument) {
    return self->add_source(
      // Tell remote side what topics we have subscribers to.
      std::forward_as_tuple(handshake_argument),
      // initialize state
      [](unit_t&) {
        // nop
      },
      // get next element
      [ptr](unit_t&, downstream<int>& out, size_t num) mutable {
        auto& xs = ptr->out.buf;
        auto n = std::min(num, xs.size());
        if (n > 0) {
          for (size_t i = 0; i < n; ++i)
            out.push(xs[i]);
          xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
        }
      },
      // never done 
      [](const unit_t&) {
        return false;
      },
      // use our custom policy
      core_broadcast_policy::make(*this)
    );
  }

  template <class T>
  stream<int> new_stream(const strong_actor_ptr& dest, const peer_ptr& ptr,
                         const T& handshake_argument) {
    return self->new_stream(
      dest,
      // Tell remote side what topics we have subscribers to.
      std::forward_as_tuple(handshake_argument),
      // initialize state
      [](unit_t&) {
        // nop
      },
      // get next element
      [ptr](unit_t&, downstream<int>& out, size_t num) mutable {
        auto& xs = ptr->out.buf;
        auto n = std::min(num, xs.size());
        if (n > 0) {
          for (size_t i = 0; i < n; ++i)
            out.push(xs[i]);
          xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
        }
      },
      // never done 
      [](const unit_t&) {
        return false;
      },
      [=](expected<void>) {
        // 
      },
      // use our custom policy
      core_broadcast_policy::make(*this)
    );
  }

  stream_handler_ptr sink(const stream<int>& in, const peer_ptr& ptr) {
    return self->add_sink(
      // input stream
      in,
      // initialize state
      [](unit_t&) {
        // nop
      },
      // processing step 
      [ptr](unit_t&, int) mutable {
         // TODO: implement me
      },
      // cleanup and produce result message
      [](unit_t&) {
        // nop
      }
    ).ptr();
  }

  /// Streams to and from peers.
  std::map<strong_actor_ptr, peer_ptr> streams;

  /// List of pending peering requests, i.e., state established after
  /// receiving {`peer`, topics} (step #1) but before receiving the actual
  /// stream handshake (step #3).
  std::map<strong_actor_ptr, peer_ptr> pending;

  /// Requested topics on this core.
  topics filter;

  /// Manages local subscribers (a stream with downstream paths only).
  stream_handler_ptr local_receivers;

  /// Manages local senders (a stream with upstream paths only).
  stream_handler_ptr local_senders;

  /// Points to the parent actor.
  event_based_actor* self;
};

void core_broadcast_policy::push(abstract_downstream& x, size_t* hint) {
  using dtype = downstream<int>;
  CAF_ASSERT(dynamic_cast<dtype*>(&x) != nullptr);
  auto& out = static_cast<dtype&>(x);
  auto num = hint ? *hint : available_credit(x);
  auto first = out.buf().begin();
  auto last = std::next(first, static_cast<ptrdiff_t>(num));
  for (auto& kvp : st_.streams) {
    auto& ptr = kvp.second->out.ptr;
    CAF_ASSERT(ptr != nullptr);
    auto& path = static_cast<dtype&>(*ptr->get_downstream());
    auto& buf = path.buf();
    buf.insert(buf.end(), first, last);
    path.push();
  }
  out.buf().erase(first, last);
}

size_t core_broadcast_policy::available_credit(const abstract_downstream&) {
  if (st_.streams.empty())
    return 0;
  size_t res = std::numeric_limits<size_t>::max();
  for (auto& kvp : st_.streams) {
    auto& ptr = kvp.second->out.ptr;
    CAF_ASSERT(ptr != nullptr);
    res = std::min(res, ptr->get_downstream()->total_credit());
  }
  return res;
}

behavior core(stateful_actor<state>* self, topics ts) {
  self->state.filter = std::move(ts);
  return {
    // "Step #0": a local actor requests a new peering to B.
    [=](peer_atom, strong_actor_ptr remote_core) -> result<void> {
      if (remote_core == nullptr)
        return sec::invalid_argument;
      // Simply return if we already are peering with B.
      auto& st = self->state;
      auto i = st.streams.find(remote_core);
      if (i != st.streams.end())
        return unit;
      // Create necessary state and send message to remote core.
      self->send(actor{self} * actor_cast<actor>(remote_core),
                 peer_atom::value, self->state.filter);
      return unit;
    },
    // -- 3-way handshake for establishing peering streams between A and B. ----
    // -- A (this node) performs steps #1 and #3. B performs #2 and #4. --------
    // Step #1: A demands B shall establish a stream back to A. A has
    //          subscribers to the topics `ts`.
    [=](peer_atom, topics& ts) -> stream<int> {
      auto& st = self->state;
      // Reject anonymous peering requests.
      auto p = self->current_sender();
      if (!p) {
        CAF_LOG_INFO("Removed anonymous peering request.");
        return invalid_stream;
      }
      // Ignore unexpected handshakes as well as handshakes that collide
      // with an already pending handshake.
      if (st.streams.count(p) > 0 || st.pending.count(p) > 0) {
        CAF_LOG_INFO("Received peering request for already known peer.");
        return invalid_stream;
      }
      auto ptr = make_counted<peer_state>();
      auto res = st.source(ptr, self->state.filter);
      ptr->out.filter = std::move(ts);
      ptr->out.sid = res.id();
      ptr->out.ptr = res.ptr();
      st.pending.emplace(p, std::move(ptr));
      return res;
    },
    // step #2: B establishes a stream to A, sending its own local subscriptions
    [=](const stream<int>& in, topics& filter) {
      auto& st = self->state;
      // Reject anonymous peering requests and unrequested handshakes.
      auto p = st.prev_peer_from_handshake();
      if (p == nullptr) {
        CAF_LOG_INFO("Ingored anonymous peering request.");
        return;
      }
      // Initialize required state for in- and output stream.
      auto ptr = make_counted<peer_state>();
      st.sink(in, ptr);
      ptr->in.sid = in.id();
      ptr->in.ptr = self->streams().find(in.id())->second;
      auto res = st.new_stream(p, ptr, ok_atom::value);
      ptr->out.filter = std::move(filter);
      ptr->out.sid = res.id();
      ptr->out.ptr = res.ptr();
    },
    // step #3: A establishes a stream to B
    // (now B has a stream to A and vice versa)
    [=](const stream<int>& in, ok_atom) {
      auto& st = self->state;
      // Reject anonymous peering requests and unrequested handshakes.
      auto p = st.prev_peer_from_handshake();
      if (!p) {
        CAF_LOG_INFO("Ignored anonymous peering request.");
        return;
      }
      // Reject step #3 handshake if this actor didn't receive a step #1
      // handshake previously.
      auto i = st.pending.find(p);
      if (i == st.pending.end()) {
        CAF_LOG_WARNING("Received a step #3 handshake, but no #1 previously.");
        return;
      }
      // Finalize state by creating a sink and updating our peer information.
      auto& ptr = i->second;
      ptr->in.sid = in.id();
      ptr->in.ptr = st.sink(in, ptr);
      st.streams.emplace(p, std::move(ptr));
      st.pending.erase(i);
    },
    // -- Communication to local actors: incoming streams and subscriptions.----
    [=](join_atom) -> stream<int> {
      auto& st = self->state;
      if (st.local_receivers == nullptr) {
        // TODO
      }
      return invalid_stream;
    },
    [=](const stream<int>&) {

    }
  };
}

using fixture = test_coordinator_fixture<>;

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(manual_stream_management, fixture)

CAF_TEST(three_way_handshake) {
  auto core1 = sys.spawn(core, topics{"a", "b", "c"});
  auto core2 = sys.spawn(core, topics{"c", "d", "e"});
  // initiate handshake between core1 and core2
  self->send(core1, peer_atom::value, actor_cast<strong_actor_ptr>(core2));
  expect((peer_atom, strong_actor_ptr), from(self).to(core1).with(_, core2));
  // step #1: core1 ----('peer', topics)---> core2
  expect((peer_atom, topics),
         from(core1).to(core2).with(_, topics{"a", "b", "c"}));
  // step #2: core1 <---(stream_msg::open)---- core2
  expect((stream_msg::open),
         from(_).to(core1).with(
           std::make_tuple(_, topics{"c", "d", "e"}), core2, _, _,
           false));
  // step #3: core1 ----(stream_msg::open)---> core2
  //          core1 ----(stream_msg::ack_open)---> core2
  expect((stream_msg::open), from(_).to(core2).with(_, core1, _, _, false));
  expect((stream_msg::ack_open), from(core1).to(core2).with(_, 5, _, false));
  // shutdown stuff
  CAF_MESSAGE("shutdown core actors");
  anon_send_exit(core1, exit_reason::user_shutdown);
  anon_send_exit(core2, exit_reason::user_shutdown);
  sched.run();


  return;
  // core1 <----(stream_msg::ack_open)------ core2
  expect((stream_msg::ack_open), from(core2).to(core1).with(_, 5, _, false));
  // core1 ----(stream_msg::batch)---> core2
  expect((stream_msg::batch),
         from(core1).to(core2).with(5, std::vector<int>{1, 2, 3, 4, 5}, 0));
  // core1 <--(stream_msg::ack_batch)---- core2
  expect((stream_msg::ack_batch), from(core2).to(core1).with(5, 0));
  // core1 ----(stream_msg::batch)---> core2
  expect((stream_msg::batch),
         from(core1).to(core2).with(4, std::vector<int>{6, 7, 8, 9}, 1));
  // core1 <--(stream_msg::ack_batch)---- core2
  expect((stream_msg::ack_batch), from(core2).to(core1).with(4, 1));
  // core1 ----(stream_msg::close)---> core2
  expect((stream_msg::close), from(core1).to(core2).with());
  // core2 ----(result: 25)---> core1
  expect((int), from(core2).to(core1).with(45));
}

CAF_TEST_FIXTURE_SCOPE_END()
