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

#define CAF_SUITE manual_stream_management
#include "caf/test/dsl.hpp"

#include "caf/stream_source.hpp"
#include "caf/filtering_downstream.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/test_multiplexer.hpp"

using std::cout;
using std::endl;
using std::string;

using namespace caf;

// -- Type aliases -------------------------------------------------------------

using peer_atom = atom_constant<atom("peer")>;

using key_type = string;

using value_type = int;

using filter_type = std::vector<key_type>;

using element_type = std::pair<key_type, value_type>;

using stream_type = stream<element_type>;

// -- Convenience functions ----------------------------------------------------

/// Returns `true` if `x` is selected by `f`, `false` otherwise.
bool selected(const filter_type& f, const element_type& x) {
  using std::get;
  for (auto& key : f)
    if (key == get<0>(x))
      return true;
  return false;
}

// -- Class definitions --------------------------------------------------------

struct core_state;

/// A stream governor dispatches incoming data from all publishers to local
/// subscribers as well as peers. Its primary job is to avoid routing loops by
/// not forwarding data from a peer back to itself.
class stream_governor : public stream_handler {
public:
  // -- Nested types -----------------------------------------------------------

  struct peer_data {
    filter_type filter;
    downstream<element_type> out;
    stream_id incoming_sid;

    peer_data(filter_type y, local_actor* self, const stream_id& sid,
              abstract_downstream::policy_ptr pp)
        : filter(std::move(y)),
          out(self, sid, std::move(pp)) {
      // nop
    }

    template <class Inspector>
    friend typename Inspector::result_type inspect(Inspector& f, peer_data& x) {
      return f(x.filter, x.out, x.incoming_sid);
    }
  };

  using peer_data_ptr = std::unique_ptr<peer_data>;

  using peer_map = std::unordered_map<strong_actor_ptr, peer_data_ptr>;

  using local_downstream = filtering_downstream<element_type, key_type>;

  // -- Constructors and destructors -------------------------------------------

  stream_governor(core_state* state);

  // -- Accessors --------------------------------------------------------------

  inline const peer_map& peers() const {
    return peers_;
  }

  inline bool has_peer(const strong_actor_ptr& hdl) const {
    return peers_.count(hdl) > 0;
  }

  inline local_downstream& local_subscribers() {
    return local_subscribers_;
  }

  // -- Mutators ---------------------------------------------------------------
  
  template <class... Ts>
  void new_stream(const strong_actor_ptr& hdl, const stream_id& sid,
                  std::tuple<Ts...> xs) {
    CAF_ASSERT(hdl != nullptr);
    stream_type token{sid};
    auto ys = std::tuple_cat(std::make_tuple(token), std::move(xs));
    new_stream(hdl, token, make_message_from_tuple(std::move(ys)));
  }

  peer_data* add_peer(strong_actor_ptr ptr, filter_type filter);

  // -- Overridden member functions of `stream_handler` ------------------------

  error add_downstream(strong_actor_ptr& hdl) override;

  error confirm_downstream(const strong_actor_ptr& rebind_from,
                           strong_actor_ptr& hdl, long initial_demand,
                           bool redeployable) override;

  error downstream_demand(strong_actor_ptr& hdl, long new_demand) override;

  error push(long* hint) override;

  expected<long> add_upstream(strong_actor_ptr& hdl, const stream_id& sid,
                                stream_priority prio) override;

  error upstream_batch(strong_actor_ptr& hdl, long, message& xs) override;

  error close_upstream(strong_actor_ptr& hdl) override;

  void abort(strong_actor_ptr& cause, const error& reason) override;

  bool done() const override;

  message make_output_token(const stream_id&) const override;

  long total_downstream_net_credit() const;

private:
  void new_stream(const strong_actor_ptr& hdl, const stream_type& token,
                  message msg);

  core_state* state_;
  upstream<element_type> in_;
  local_downstream local_subscribers_;
  peer_map peers_;
};

struct core_state {
  /// Requested topics on this core.
  filter_type filter;
  
  /// Multiplexes local streams as well as stream for peers.
  intrusive_ptr<stream_governor> governor;

  /// List of all known publishers. Whenever we change the `filter` on a core,
  /// we need to send the updated filter to all publishers.
  std::vector<strong_actor_ptr> peers;

  /// Stream ID used by the governor.
  stream_id sid;

  /// Set of pending handshake requests.
  std::set<strong_actor_ptr> pending_peers;

  /// Pointer to the owning actor.
  event_based_actor* self;

  /// Name of this actor type.
  static const char* name;

  void init(event_based_actor* s, filter_type initial_filter) {
    self = s;
    filter = std::move(initial_filter);
    sid =
      stream_id{self->ctrl(),
                self->new_request_id(message_priority::normal).integer_value()};
    governor = make_counted<stream_governor>(this);
    self->streams().emplace(sid, governor);
  }

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
  }
};

const char* core_state::name = "core";

// -- Implementation of `stream_governor` --------------------------------------

stream_governor::stream_governor(core_state* state)
    : state_(state),
      in_(state->self, policy::greedy::make()),
      local_subscribers_(state->self, state->sid, policy::broadcast::make()) {
  // nop
}
stream_governor::peer_data* stream_governor::add_peer(strong_actor_ptr hdl,
                                                      filter_type filter) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(filter));
  abstract_downstream::policy_ptr pp{new policy::broadcast};
  auto ptr = new peer_data{std::move(filter), state_->self,
                          state_->sid, std::move(pp)};
  ptr->out.add_path(hdl);
  auto res = peers_.emplace(std::move(hdl), peer_data_ptr{ptr});
  return res.second ? ptr : nullptr;
}

error stream_governor::add_downstream(strong_actor_ptr&) {
  CAF_LOG_ERROR("add_downstream on governor called");
  return sec::invalid_stream_state;
}

error stream_governor::confirm_downstream(const strong_actor_ptr& rebind_from,
                                          strong_actor_ptr& hdl,
                                          long initial_demand,
                                          bool redeployable) {
  CAF_LOG_TRACE(CAF_ARG(rebind_from) << CAF_ARG(hdl)
                << CAF_ARG(initial_demand) << CAF_ARG(redeployable));
  CAF_IGNORE_UNUSED(redeployable);
  auto path = local_subscribers_.find(rebind_from);
  if (path) {
    if (!local_subscribers_.confirm_path(rebind_from, hdl, initial_demand)) {
      CAF_LOG_ERROR("Cannot rebind to registered downstream.");
      return sec::invalid_stream_state;
    }
    return downstream_demand(hdl, initial_demand);
  }
  auto i = peers_.find(rebind_from);
  if (i != peers_.end()) {
    auto uptr = std::move(i->second);
    peers_.erase(i);
    auto res = peers_.emplace(hdl, std::move(uptr));
    if (!res.second) {
      CAF_LOG_ERROR("Cannot rebind to registered downstream.");
      return sec::invalid_stream_state;
    }
    CAF_LOG_DEBUG("Confirmed path to another core"
                 << CAF_ARG(rebind_from) << CAF_ARG(hdl));
    res.first->second->out.confirm_path(rebind_from, hdl, initial_demand);
    return downstream_demand(hdl, initial_demand);
  }
  CAF_LOG_ERROR("Cannot confirm path to unknown downstream.");
  return sec::invalid_downstream;
}

error stream_governor::downstream_demand(strong_actor_ptr& hdl, long value) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(value));
  auto path = local_subscribers_.find(hdl);
  if (path) {
    path->open_credit += value;
    return push(nullptr);
  }
  auto i = peers_.find(hdl);
  if (i != peers_.end()) {
    auto pp = i->second->out.find(hdl);
    if (!pp)
      return sec::invalid_stream_state;
    CAF_LOG_DEBUG("grant" << value << "new credit to" << hdl);
    pp->open_credit += value;
    return push(nullptr);
  }
  return sec::invalid_downstream;
}

error stream_governor::push(long* hint) {
  CAF_LOG_TRACE("");
  if (local_subscribers_.buf_size() > 0)
    local_subscribers_.policy().push(local_subscribers_, hint);
  for (auto& kvp : peers_) {
    auto& out = kvp.second->out;
    if (out.buf_size() > 0)
      out.policy().push(out, hint);
  }
  return none;
}

expected<long> stream_governor::add_upstream(strong_actor_ptr& hdl,
                                               const stream_id& sid,
                                               stream_priority prio) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(sid) << CAF_ARG(prio));
  if (hdl)
    return in_.add_path(hdl, sid, prio, total_downstream_net_credit());
  return sec::invalid_argument;
}

error stream_governor::upstream_batch(strong_actor_ptr& hdl, long xs_size,
                                      message& xs) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(xs_size) << CAF_ARG(xs));
  using std::get;
  // Sanity checking.
  auto path = in_.find(hdl);
  if (!path)
    return sec::invalid_upstream;
  if (xs_size > path->assigned_credit)
    return sec::invalid_stream_state;
  if (!xs.match_elements<std::vector<element_type>>())
    return sec::unexpected_message;
  // Unwrap `xs`.
  auto& vec = xs.get_mutable_as<std::vector<element_type>>(0);
  // Decrease credit assigned to `hdl` and get currently available downstream
  // credit on all paths.
  CAF_LOG_DEBUG(CAF_ARG(path->assigned_credit));
  path->assigned_credit -= xs_size;
  // Forward data to all other peers.
  for (auto& kvp : peers_)
    if (kvp.first != hdl) {
      auto& out = kvp.second->out;
      for (const auto& x : vec)
        if (selected(kvp.second->filter, x))
          out.push(x);
      if (out.buf_size() > 0) {
        out.policy().push(out);
      }
    }
  // Move elements from `xs` to the buffer for local subscribers.
  for (auto& x : vec)
    local_subscribers_.push(std::move(x));
  local_subscribers_.policy().push(local_subscribers_);
  // Grant new credit to upstream if possible.
  auto available = total_downstream_net_credit();
  if (available > 0)
    in_.assign_credit(available);
  return none;
}

error stream_governor::close_upstream(strong_actor_ptr& hdl) {
  CAF_LOG_TRACE(CAF_ARG(hdl));
  if (in_.remove_path(hdl))
    return none;
  return sec::invalid_upstream;
}

void stream_governor::abort(strong_actor_ptr& hdl, const error& reason) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(reason));
  CAF_IGNORE_UNUSED(reason);
  if (local_subscribers_.remove_path(hdl))
    return;
  auto i = peers_.find(hdl);
  if (i != peers_.end()) {
    auto& pd = *i->second;
    state_->self->streams().erase(pd.incoming_sid);
    peers_.erase(i);
  }
}

bool stream_governor::done() const {
  return false;
}

message stream_governor::make_output_token(const stream_id& x) const {
  return make_message(stream<element_type>{x});
}

long stream_governor::total_downstream_net_credit() const {
  auto net_credit = local_subscribers_.total_net_credit();
  for (auto& kvp : peers_)
    net_credit = std::min(net_credit, kvp.second->out.total_net_credit());
  return net_credit;
}

void stream_governor::new_stream(const strong_actor_ptr& hdl,
                                 const stream_type& token, message msg) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(token) << CAF_ARG(msg));
  CAF_ASSERT(hdl != nullptr);
  auto self = state_->self;
  hdl->enqueue(make_mailbox_element(self->ctrl(), message_id::make(), {},
                                    make<stream_msg::open>(
                                      token.id(), std::move(msg), self->ctrl(),
                                      hdl, stream_priority::normal, false)),
               self->context());
  self->streams().emplace(token.id(), this);
}

// -- Implementation of core actor ---------------------------------------------

namespace {

behavior core(stateful_actor<core_state>* self, filter_type initial_filter) {
  self->state.init(self, std::move(initial_filter));
  return {
    // -- Peering requests from local actors, i.e., "step 0". ------------------
    [=](peer_atom, strong_actor_ptr remote_core) -> result<void> {
      auto& st = self->state;
      // Sanity checking.
      if (remote_core == nullptr)
        return sec::invalid_argument;
      // Create necessary state and send message to remote core if not already
      // peering with B.
      if (!st.governor->has_peer(remote_core))
        self->send(actor{self} * actor_cast<actor>(remote_core),
                   peer_atom::value, st.filter);
      return unit;
    },
    // -- 3-way handshake for establishing peering streams between A and B. ----
    // -- A (this node) performs steps #1 and #3. B performs #2 and #4. --------
    // Step #1: A demands B shall establish a stream back to A. A has
    //          subscribers to the topics `ts`.
    [=](peer_atom, filter_type& peer_ts) -> stream_type {
      auto& st = self->state;
      // Reject anonymous peering requests.
      auto p = self->current_sender();
      if (p == nullptr) {
        CAF_LOG_DEBUG("Drop anonymous peering request.");
        return invalid_stream;
      }
      // Ignore unexpected handshakes as well as handshakes that collide
      // with an already pending handshake.
      if (st.pending_peers.count(p) > 0) {
        CAF_LOG_DEBUG("Drop repeated peering request.");
        return invalid_stream;
      }
      auto peer_ptr = st.governor->add_peer(p, std::move(peer_ts));
      if (peer_ptr == nullptr) {
        CAF_LOG_DEBUG("Drop peering request of already known peer.");
        return invalid_stream;
      }
      st.pending_peers.emplace(std::move(p));
      auto& next = self->current_mailbox_element()->stages.back();
      CAF_ASSERT(next != nullptr);
      auto token = std::make_tuple(st.filter);
      self->fwd_stream_handshake<element_type>(st.sid, token);
      return {st.sid, st.governor};
    },
    // step #2: B establishes a stream to A, sending its own local subscriptions
    [=](const stream_type& in, filter_type& filter) {
      auto& st = self->state;
      // Reject anonymous peering requests and unrequested handshakes.
      auto p = st.prev_peer_from_handshake();
      if (p == nullptr) {
        CAF_LOG_DEBUG("Drop anonymous peering request.");
        return;
      }
      // Ignore duplicates.
      if (st.governor->has_peer(p)) {
        CAF_LOG_DEBUG("Drop repeated handshake phase #2.");
        return;
      }
      // Add state to actor.
      auto peer_ptr = st.governor->add_peer(p, std::move(filter));
      peer_ptr->incoming_sid = in.id();
      self->streams().emplace(in.id(), st.governor);
      // Start streaming in opposite direction.
      st.governor->new_stream(p, st.sid, std::make_tuple(ok_atom::value));
    },
    // step #3: A establishes a stream to B
    // (now B has a stream to A and vice versa)
    [=](const stream_type& in, ok_atom) {
      CAF_LOG_TRACE(CAF_ARG(in));
      auto& st = self->state;
      // Reject anonymous peering requests and unrequested handshakes.
      auto p = st.prev_peer_from_handshake();
      if (p == nullptr) {
        CAF_LOG_DEBUG("Ignored anonymous peering request.");
        return;
      }
      // Reject step #3 handshake if this actor didn't receive a step #1
      // handshake previously.
      auto i = st.pending_peers.find(p);
      if (i == st.pending_peers.end()) {
        CAF_LOG_WARNING("Received a step #3 handshake, but no #1 previously.");
        return;
      }
      st.pending_peers.erase(i);
      auto res = self->streams().emplace(in.id(), st.governor);
      if (!res.second) {
        CAF_LOG_WARNING("Stream already existed.");
      }
    },
    // -- Communication to local actors: incoming streams and subscriptions. ---
    [=](join_atom, filter_type& filter) -> expected<stream_type> {
      auto& st = self->state;
      auto& cs = self->current_sender();
      if (cs == nullptr)
        return sec::cannot_add_downstream;
      auto& stages = self->current_mailbox_element()->stages;
      if (stages.empty()) {
        CAF_LOG_ERROR("Cannot join a data stream without downstream.");
        auto rp = self->make_response_promise();
        rp.deliver(sec::no_downstream_stages_defined);
        return stream_type{stream_id{nullptr, 0}, nullptr};
      }
      auto next = stages.back();
      CAF_ASSERT(next != nullptr);
      std::tuple<> token;
      self->fwd_stream_handshake<element_type>(st.sid, token);
      st.governor->local_subscribers().add_path(cs);
      st.governor->local_subscribers().set_filter(cs, std::move(filter));
      return stream_type{st.sid, st.governor};
    },
    [=](const stream_type& in) {
      auto& st = self->state;
      auto& cs = self->current_sender();
      if (cs == nullptr) {
        return;
      }
      self->streams().emplace(in.id(), st.governor);
    }
  };
}

void driver(event_based_actor* self, const actor& sink) {
  using buf_type = std::vector<element_type>;
  self->new_stream(
    // Destination.
    sink,
    // Initialize send buffer with 10 elements.
    [](buf_type& xs) {
      xs = buf_type{{"a", 0}, {"b", 0}, {"a", 1}, {"a", 2}, {"b", 1},
                    {"b", 2}, {"a", 3}, {"b", 3}, {"a", 4}, {"a", 5}};
    },
    // Get next element.
    [](buf_type& xs, downstream<element_type>& out, size_t num) {
      auto n = std::min(num, xs.size());
      for (size_t i = 0u; i < n; ++i)
        out.push(xs[i]);
      xs.erase(xs.begin(), xs.begin() + static_cast<ptrdiff_t>(n));
    },
    // Did we reach the end?.
    [](const buf_type& xs) {
      return xs.empty();
    },
    // Handle result of the stream.
    [](expected<void>) {
      // nop
    }
  );
}

struct consumer_state {
  std::vector<element_type> xs;
};

void consumer(stateful_actor<consumer_state>* self, filter_type ts,
              const actor& src) {
  self->send(self * src, join_atom::value, std::move(ts));
  self->become(
    [=](const stream_type& in) {
      self->add_sink(
        // Input stream.
        in,
        // Initialize state.
        [](unit_t&) {
          // nop
        },
        // Process single element.
        [=](unit_t&, element_type x) {
          self->state.xs.emplace_back(std::move(x));
        },
        // Cleanup.
        [](unit_t&) {
          // nop
        }
      );
    },
    [=](get_atom) {
      return self->state.xs;
    }
  );
}

struct config : actor_system_config {
public:
  config() {
    add_message_type<element_type>("element");
    logger_filename = "streamlog";
  }
};

using fixture = test_coordinator_fixture<config>;

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(manual_stream_management, fixture)

CAF_TEST(two_peers) {
  // Spawn core actors.
  auto core1 = sys.spawn(core, filter_type{"a", "b", "c"});
  auto core2 = sys.spawn(core, filter_type{"a", "b", "c"});
  sched.run();
  // Connect a consumer (leaf) to core2.
  auto leaf = sys.spawn(consumer, filter_type{"b"}, core2);
  sched.run_once();
  expect((atom_value, filter_type),
         from(leaf).to(core2).with(join_atom::value, filter_type{"b"}));
  expect((stream_msg::open), from(_).to(leaf).with(_, core2, _, _, false));
  expect((stream_msg::ack_open), from(leaf).to(core2).with(_, 5, _, false));
  // Initiate handshake between core1 and core2.
  self->send(core1, peer_atom::value, actor_cast<strong_actor_ptr>(core2));
  expect((peer_atom, strong_actor_ptr), from(self).to(core1).with(_, core2));
  // Step #1: core1  --->    ('peer', filter_type)    ---> core2
  expect((peer_atom, filter_type),
         from(core1).to(core2).with(_, filter_type{"a", "b", "c"}));
  // Step #2: core1  <---   (stream_msg::open)   <--- core2
  expect((stream_msg::open),
         from(_).to(core1).with(
           std::make_tuple(_, filter_type{"a", "b", "c"}), core2, _, _,
           false));
  // Step #3: core1  --->   (stream_msg::open)   ---> core2
  //          core1  ---> (stream_msg::ack_open) ---> core2
  expect((stream_msg::open), from(_).to(core2).with(_, core1, _, _, false));
  expect((stream_msg::ack_open), from(core1).to(core2).with(_, 5, _, false));
  expect((stream_msg::ack_open), from(core2).to(core1).with(_, 5, _, false));
  // There must be no communication pending at this point.
  CAF_REQUIRE(!sched.has_job());
  // Spin up driver on core1.
  auto d1 = sys.spawn(driver, core1);
  sched.run_once();
  expect((stream_msg::open), from(_).to(core1).with(_, d1, _, _, false));
  expect((stream_msg::ack_open), from(core1).to(d1).with(_, 5, _, false));
  // Data flows from driver to core1 to core2 and finally to leaf.
  using buf = std::vector<element_type>;
  expect((stream_msg::batch),
         from(d1).to(core1)
         .with(5, buf{{"a", 0}, {"b", 0}, {"a", 1}, {"a", 2}, {"b", 1}}, 0));
  expect((stream_msg::batch),
         from(core1).to(core2)
         .with(5, buf{{"a", 0}, {"b", 0}, {"a", 1}, {"a", 2}, {"b", 1}}, 0));
  expect((stream_msg::batch),
         from(core2).to(leaf)
         .with(2, buf{{"b", 0}, {"b", 1}}, 0));
  expect((stream_msg::ack_batch), from(core2).to(core1).with(5, 0));
  expect((stream_msg::ack_batch), from(core1).to(d1).with(5, 0));
  // Check log of the consumer.
  self->send(leaf, get_atom::value);
  sched.prioritize(leaf);
  sched.run_once();
  self->receive(
    [](const buf& xs) {
      buf expected{{"b", 0}, {"b", 1}};
      CAF_REQUIRE_EQUAL(xs, expected);
    }
  );
  // Shutdown.
  CAF_MESSAGE("Shutdown core actors.");
  anon_send_exit(core1, exit_reason::user_shutdown);
  anon_send_exit(core2, exit_reason::user_shutdown);
  anon_send_exit(leaf, exit_reason::user_shutdown);
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
