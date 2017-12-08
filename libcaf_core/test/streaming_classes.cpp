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

// This test simulates a complex multiplexing over multiple layers of WDRR
// scheduled queues. The goal is to reduce the complex mailbox management of
// CAF to its bare bones in order to test whether the multiplexing of stream
// traffic and asynchronous messages works as intended.
//
// The setup is a fixed WDRR queue with three nestes queues. The first nested
// queue stores asynchronous messages, the second one upstream messages, and
// the last queue is a dynamic WDRR queue storing downstream messages.
//
// We mock just enough of an actor to use the streaming classes and put them to
// work in a pipeline with 2 or 3 stages.

#define CAF_SUITE streaming_classes

#include <memory>
#include <numeric>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/broadcast_scatterer.hpp"
#include "caf/buffered_scatterer.hpp"
#include "caf/downstream_msg.hpp"
#include "caf/inbound_path.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/no_stages.hpp"
#include "caf/outbound_path.hpp"
#include "caf/send.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_scatterer.hpp"
#include "caf/stream_slot.hpp"
#include "caf/system_messages.hpp"
#include "caf/upstream_msg.hpp"
#include "caf/variant.hpp"

#include "caf/policy/arg.hpp"

#include "caf/mixin/sender.hpp"

#include "caf/test/unit_test.hpp"

#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/singly_linked.hpp"
#include "caf/intrusive/task_result.hpp"
#include "caf/intrusive/wdrr_dynamic_multiplexed_queue.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"

#include "caf/detail/gcd.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/stream_sink_impl.hpp"
#include "caf/detail/stream_source_impl.hpp"
#include "caf/detail/stream_stage_impl.hpp"
#include "caf/detail/tick_emitter.hpp"

using std::vector;

using namespace caf;
using namespace caf::intrusive;

namespace {

// -- utility ------------------------------------------------------------------

struct print_with_comma_t {
  bool first = true;
  template <class T>
  std::ostream&  operator()(std::ostream& out, const T& x) {
    if (!first)
      out << ", ";
    else
      first = false;
    return out << x;
  }

  template <class T>
  std::ostream& operator()(std::ostream& out, const logger::arg_wrapper<T>& x) {
    if (!first)
      out << ", ";
    else
      first = false;
    return out << x.name << " = " << deep_to_string(x.value);
  }
};

template <class T, class... Ts>
std::string collapse_args(const T& x, const Ts&... xs) {
  std::ostringstream out;
  print_with_comma_t f;
  f(out, x);
  unit(f(out, xs)...);
  return out.str();
}

#define TRACE(name, type, ...)                                                 \
  CAF_MESSAGE(name << " received a " << #type << ": "                          \
                   << collapse_args(__VA_ARGS__));

const char* name_of(const strong_actor_ptr& x) {
  CAF_ASSERT(x != nullptr);
  auto ptr = actor_cast<abstract_actor*>(x);
  return static_cast<local_actor*>(ptr)->name();
}

const char* name_of(const actor_addr& x) {
  return name_of(actor_cast<strong_actor_ptr>(x));
}

// -- policies and queues ------------------------------------------------------

struct policy_base {
  using mapped_type = mailbox_element;

  using task_size_type = size_t;

  using deficit_type = size_t;

  using deleter_type = detail::disposer;

  using unique_pointer = mailbox_element_ptr;
};

struct default_queue_policy : policy_base {
  static inline task_size_type task_size(const mailbox_element&) {
    return 1;
  }
};

using default_queue = drr_queue<default_queue_policy>;

struct umsg_queue_policy : policy_base {
  stream_manager* mgr;
  umsg_queue_policy(stream_manager* ptr) : mgr(ptr) {
    // nop
  }
  static inline task_size_type task_size(const mailbox_element&) {
    return 1;
  }
};

using umsg_queue = drr_queue<umsg_queue_policy>;

struct inner_dmsg_queue_policy : policy_base {
  using key_type = stream_slot;

  task_size_type task_size(const mailbox_element& x) {
    return visit(*this, x.content().get_as<downstream_msg>(0).content);
  }

  task_size_type operator()(const downstream_msg::batch& x) const {
    CAF_ASSERT(x.xs_size > 0);
    return static_cast<task_size_type>(x.xs_size);
  }

  template <class T>
  task_size_type operator()(const T&) const {
    return 1;
  }

  inner_dmsg_queue_policy(std::unique_ptr<inbound_path> ptr)
      : handler(std::move(ptr)) {
    // nop
  }

  std::unique_ptr<inbound_path> handler;
};

using inner_dmsg_queue = drr_queue<inner_dmsg_queue_policy>;

struct dmsg_queue_policy : policy_base {
  using key_type = stream_slot;

  using queue_map_type = std::map<stream_slot, inner_dmsg_queue>;

  key_type id_of(mailbox_element& x) {
    return x.content().get_as<downstream_msg>(0).slots.receiver;
  }

  template <class Queue>
  static inline bool enabled(const Queue& q) {
    return !q.policy().handler->mgr->congested();
  }

  template <class Queue>
  deficit_type quantum(const Queue&, deficit_type x) {
    return x;
  }
};

using dmsg_queue = wdrr_dynamic_multiplexed_queue<dmsg_queue_policy>;

struct mboxpolicy : policy_base {
  template <class Queue>
  deficit_type quantum(const Queue&, deficit_type x) {
    return x;
  }

  size_t id_of(const mailbox_element& x) {
    return x.mid.category();
  }
};

using mboxqueue = wdrr_fixed_multiplexed_queue<mboxpolicy, default_queue,
                                                umsg_queue, dmsg_queue,
                                                default_queue>;

// -- entity -------------------------------------------------------------------

class abstract_clock {
public:
  // -- member types -----------------------------------------------------------

  using time_point = std::chrono::steady_clock::time_point;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~abstract_clock() {
    // nop
  }

  virtual time_point now() const noexcept = 0;
};

class fake_clock : public abstract_clock {
public:
  fake_clock(time_point* global_time) : global_time_(global_time) {
    // nop
  }

  time_point now() const noexcept override {
    return *global_time_;
  }

private:
  time_point* global_time_;
};

class steady_clock : public abstract_clock {
public:
  time_point now() const noexcept override {
    return std::chrono::steady_clock::now();
  }
};

class entity : public extend<local_actor, entity>::with<mixin::sender> {
public:
  // -- member types -----------------------------------------------------------

  /// Base type.
  using super = extend<local_actor, entity>::with<mixin::sender>;

  /// Defines the messaging interface.
  using signatures = none_t;

  /// Defines the container for storing message handlers.
  using behavior_type = behavior;

  /// The type of a single tick.
  using clock_type = detail::tick_emitter::clock_type;

  /// The type of a single tick.
  using time_point = clock_type::time_point;

  /// Difference between two points in time.
  using duration_type = time_point::duration;

  /// The type of a single tick.
  using tick_type = long;

  // -- constructors, destructors, and assignment operators --------------------

  entity(actor_config& cfg, const char* cstr_name, time_point* global_time,
         duration_type credit_interval, duration_type force_batches_interval)
      : super(cfg),
        mbox(mboxpolicy{}, default_queue_policy{}, nullptr, dmsg_queue_policy{},
             default_queue_policy{}),
        name_(cstr_name),
        next_slot_(static_cast<stream_slot>(id())),
        global_time_(global_time),
        tick_emitter_(global_time == nullptr ? clock_type::now()
                                             : *global_time) {
    auto cycle = detail::gcd(credit_interval.count(),
                             force_batches_interval.count());
    ticks_per_force_batches_interval = force_batches_interval.count() / cycle;
    ticks_per_credit_interval = credit_interval.count() / cycle;
    tick_emitter_.interval(duration_type{cycle});
  }

  void enqueue(mailbox_element_ptr what, execution_unit*) override {
    auto push_back_result = mbox.push_back(std::move(what));
    CAF_CHECK_EQUAL(push_back_result, true);
    CAF_ASSERT(push_back_result);
  }

  void attach(attachable_ptr) override {
    // nop
  }

  size_t detach(const attachable::token&) override {
    return 0;
  }

  void add_link(abstract_actor*) override {
    // nop
  }

  void remove_link(abstract_actor*) override {
    // nop
  }

  bool add_backlink(abstract_actor*) override {
    return false;
  }

  bool remove_backlink(abstract_actor*) override {
    return false;
  }

  const char* name() const override {
    return name_;
  }

  void launch(execution_unit*, bool, bool) override {
    // nop
  }

  execution_unit* context() {
    return nullptr;
  }

  void start_streaming(entity& ref, int num_messages) {
    CAF_REQUIRE_NOT_EQUAL(num_messages, 0);
    auto slot = next_slot_++;
    CAF_MESSAGE(name_ << " starts streaming to " << ref.name()
                << " on slot " << slot);
    strong_actor_ptr to = ref.ctrl();
    send(to, open_stream_msg{slot, make_message(), ctrl(), nullptr,
                             stream_priority::normal, false});
    auto init = [](int& x) {
      x = 0;
    };
    auto f = [=](int& x, downstream<int>& out, size_t hint) {
      auto y = std::min(num_messages, x + static_cast<int>(hint));
      while (x < y)
        out.push(x++);
    };
    auto fin = [=](const int& x) {
      return x == num_messages;
    };
    policy::arg<broadcast_scatterer<int>> token;
    auto ptr = detail::make_stream_source(this, init, f, fin, token);
    ptr->generate_messages();
    pending_managers_.emplace(slot, std::move(ptr));
  }

  void forward_to(entity& ref) {
    auto slot = next_slot_++;
    CAF_MESSAGE(name_ << " starts forwarding to " << ref.name()
                << " on slot " << slot);
    strong_actor_ptr to = ref.ctrl();
    send(to, open_stream_msg{slot, make_message(), ctrl(), nullptr,
                             stream_priority::normal, false});
    using log_ptr = vector<int>*;
    auto init = [&](log_ptr& ptr) {
      ptr = &data;
    };
    auto f = [](log_ptr& ptr, downstream<int>& out, int x) {
      ptr->push_back(x);
      out.push(x);
    };
    auto cleanup = [](log_ptr&) {
      // nop
    };
    policy::arg<broadcast_scatterer<int>> token;
    forwarder = detail::make_stream_stage(this, init, f, cleanup, token);
    pending_managers_.emplace(slot, forwarder);
  }

  void operator()(open_stream_msg& hs) {
    TRACE(name_, stream_handshake_msg,
          CAF_ARG2("sender", name_of(hs.prev_stage)));
    auto slot = next_slot_++;
    //stream_slots id{slot, hs.sender_slot};
    stream_slots id{hs.slot, slot};
    // Create required state if no forwarder exists yet, otherwise `forward_to`
    // was called and we run as a stage.
    auto mgr = forwarder;
    if (mgr == nullptr) {
      using log_ptr = vector<int>*;
      auto init = [&](log_ptr& ptr) {
        ptr = &data;
      };
      auto f = [](log_ptr& ptr, int x) {
        ptr->push_back(x);
      };
      auto fin = [](log_ptr&) {
        // nop
      };
      mgr = detail::make_stream_sink(this, std::move(init), std::move(f),
                                     std::move(fin));
    }
    // mgr->out().add_path(id, hs.prev_stage);
    managers_.emplace(id, mgr);
    // Create a new queue in the mailbox for incoming traffic.
    auto ip = new inbound_path(mgr, id, hs.prev_stage);
    get<2>(mbox.queues())
      .queues()
      .emplace(slot, std::unique_ptr<inbound_path>{ip});
    ip->emit_ack_open(this, actor_cast<actor_addr>(hs.original_stage), false);
  }

  void operator()(stream_slots slots, actor_addr& sender,
                  upstream_msg::ack_open& x) {
    TRACE(name_, ack_open, CAF_ARG(slots),
          CAF_ARG2("sender", name_of(x.rebind_to)), CAF_ARG(x));
    // Get the manager for that stream.
    auto i = pending_managers_.find(slots.receiver);
    CAF_REQUIRE_NOT_EQUAL(i, pending_managers_.end());
    // Create a new queue in the mailbox for incoming traffic.
    // Swap the buddy/receiver perspective to generate the ID we are using.
    managers_.emplace(slots, i->second);
    auto to = actor_cast<strong_actor_ptr>(sender);
    CAF_REQUIRE_NOT_EQUAL(to, nullptr);
    auto out = i->second->out().add_path(slots.invert(), to);
    out->open_credit = x.initial_demand;
    out->desired_batch_size = x.desired_batch_size;
    i->second->generate_messages();
    i->second->push();
    pending_managers_.erase(i);
  }

  void operator()(stream_slots input_slots, actor_addr& sender,
                  upstream_msg::ack_batch& x) {
    TRACE(name_, ack_batch, CAF_ARG(input_slots),
          CAF_ARG2("sender", name_of(sender)), CAF_ARG(x));
    // Get the manager for that stream.
    auto i = managers_.find(input_slots);
    CAF_REQUIRE_NOT_EQUAL(i, managers_.end());
    auto to = actor_cast<strong_actor_ptr>(sender);
    CAF_REQUIRE_NOT_EQUAL(to, nullptr);
    auto out = i->second->out().path(input_slots.invert());
    CAF_REQUIRE_NOT_EQUAL(out, nullptr);
    out->open_credit += x.new_capacity;
    out->desired_batch_size = x.desired_batch_size;
    out->next_ack_id = x.acknowledged_id + 1;
    i->second->generate_messages();
    i->second->push();
    if (i->second->done()) {
      CAF_MESSAGE(name_ << " is done sending batches");
      i->second->close();
      managers_.erase(i);
    }
  }

  void advance_time() {
    auto cycle = std::chrono::milliseconds(100);
    auto desired_batch_complexity = std::chrono::microseconds(50);
    auto f = [&](tick_type x) {
      if (x % ticks_per_force_batches_interval == 0) {
        // Force batches on all output paths.
        for (auto& kvp : managers_)
          kvp.second->out().force_emit_batches();
      }
      if (x % ticks_per_credit_interval == 0) {
        // Fill credit on each input path up to 30.
        auto& qs = get<2>(mbox.queues()).queues();
        for (auto& kvp : qs) {
          auto inptr = kvp.second.policy().handler.get();
          inptr->emit_ack_batch(this, kvp.second.total_task_size(), cycle,
                                desired_batch_complexity);
        }
      }
    };
    tick_emitter_.update(now(), f);
  }

  time_point now() {
    return global_time_ == nullptr ? clock_type::now() : *global_time_;
  }

  // -- member variables -------------------------------------------------------

  mboxqueue mbox;
  const char* name_;
  stream_slot next_slot_ = 1;
  vector<int> data; // Keeps track of all received data from all batches.
  stream_manager_ptr forwarder;
  std::map<stream_slots, stream_manager_ptr> managers_;
  std::map<stream_slot, stream_manager_ptr> pending_managers_;

  tick_type ticks_per_force_batches_interval;
  tick_type ticks_per_credit_interval;
  time_point* global_time_;
  detail::tick_emitter tick_emitter_;
};

struct msg_visitor {
  // -- member types -----------------------------------------------------------

  using is_default_async = std::integral_constant<size_t, 0>;

  using is_umsg = std::integral_constant<size_t, 1>;

  using is_dmsg = std::integral_constant<size_t, 2>;

  using is_urgent_async = std::integral_constant<size_t, 3>;

  using result_type = intrusive::task_result;

  // -- operator() overloads ---------------------------------------------------

  result_type operator()(is_default_async, default_queue&, mailbox_element& x) {
    CAF_REQUIRE_EQUAL(x.content().type_token(),
                      make_type_token<open_stream_msg>());
    (*self)(x.content().get_mutable_as<open_stream_msg>(0));
    return intrusive::task_result::resume;
  }

  result_type operator()(is_urgent_async, default_queue& q,
                         mailbox_element& x) {
    is_default_async token;
    return (*this)(token, q, x);
  }

  result_type operator()(is_umsg, umsg_queue&, mailbox_element& x) {
    CAF_REQUIRE(x.content().type_token() == make_type_token<upstream_msg>());
    auto& um = x.content().get_mutable_as<upstream_msg>(0);
    auto f = detail::make_overload(
      [&](upstream_msg::ack_open& y) {
        (*self)(um.slots, um.sender, y);
      },
      [&](upstream_msg::ack_batch& y) {
        (*self)(um.slots, um.sender, y);
      },
      [](upstream_msg::drop&) {
        CAF_FAIL("did not expect upstream_msg::drop");
      },
      [](upstream_msg::forced_drop&) {
        CAF_FAIL("did not expect upstream_msg::forced_drop");
      }
    );
    visit(f, um.content);
    return intrusive::task_result::resume;
  }

  result_type operator()(is_dmsg, dmsg_queue& qs, stream_slot,
                         inner_dmsg_queue& q, mailbox_element& x) {
    CAF_REQUIRE(x.content().type_token() == make_type_token<downstream_msg>());
    auto inptr = q.policy().handler.get();
    if (inptr == nullptr)
      return intrusive::task_result::stop;
    auto& dm = x.content().get_mutable_as<downstream_msg>(0);
    auto f = detail::make_overload(
      [&](downstream_msg::batch& y) {
        TRACE(self->name(), batch, CAF_ARG2("size", y.xs_size),
              CAF_ARG2("remaining_credit", inptr->assigned_credit - y.xs_size));
        inptr->handle(y);
        if (inptr->mgr->done()) {
          CAF_MESSAGE(self->name()
                      << " is done receiving and closes its manager");
          inptr->mgr->close();
        }
        return intrusive::task_result::resume;
      },
      [&](downstream_msg::close& y) {
        TRACE(self->name(), close, CAF_ARG(dm.slots));
        auto slots = dm.slots;
        auto i = self->managers_.find(slots);
        CAF_REQUIRE_NOT_EQUAL(i, self->managers_.end());
        i->second->handle(inptr, y);
        q.policy().handler.reset();
        qs.erase_later(slots.receiver);
        if (!i->second->done()) {
          self->managers_.erase(i);
        } else {
          // Close the manager and remove it on all registered slots.
          auto mgr = i->second;
          mgr->close();
          auto j = self->managers_.begin();
          while (j != self->managers_.end()) {
            if (j->second == mgr)
              j = self->managers_.erase(j);
            else
              ++j;
          }
        }
        return intrusive::task_result::resume;
      },
      [](downstream_msg::forced_close&) {
        CAF_FAIL("did not expect downstream_msg::forced_close");
        return intrusive::task_result::stop;
      });
    return visit(f, dm.content);
  }

  // -- member variables -------------------------------------------------------

  entity* self;
};

// -- fixture ------------------------------------------------------------------

struct fixture {
  struct timing_config {
    using clock_type = std::chrono::steady_clock;

    clock_type::time_point global_time;

    clock_type::duration credit_interval = std::chrono::milliseconds(100);

    clock_type::duration force_batches_interval = std::chrono::milliseconds(50);

    clock_type::duration step = force_batches_interval;
  };

  timing_config tc;

  actor_system_config cfg;
  actor_system sys{cfg};
  actor alice_hdl;
  actor bob_hdl;
  actor carl_hdl;

  entity& alice;
  entity& bob;
  entity& carl;

  static actor spawn(actor_system& sys, actor_id id, const char* name,
                     timing_config& tc) {
    actor_config conf;
    return make_actor<entity>(id, node_id{}, &sys, conf, name, &tc.global_time,
                              tc.credit_interval, tc.force_batches_interval);
  }

  static entity& fetch(const actor& hdl) {
    return *static_cast<entity*>(actor_cast<abstract_actor*>(hdl));
  }

  fixture()
      : alice_hdl(spawn(sys, 0, "alice", tc)),
        bob_hdl(spawn(sys, 1, "bob", tc)),
        carl_hdl(spawn(sys, 2, "carl", tc)),
        alice(fetch(alice_hdl)),
        bob(fetch(bob_hdl)),
        carl(fetch(carl_hdl)) {
    // nop
  }

  ~fixture() {
    // Check whether all actors cleaned up their state properly.
    entity* xs[] = {&alice, &bob, &carl};
    for (auto x : xs) {
      CAF_CHECK(get<2>(x->mbox.queues()).queues().empty());
      CAF_CHECK(x->pending_managers_.empty());
      CAF_CHECK(x->managers_.empty());
    }
  }

  template <class... Ts>
  void loop(Ts&... xs) {
    msg_visitor fs[] = {{&xs}...};
    auto mailbox_empty = [](msg_visitor& x) { return x.self->mbox.empty(); };
    while (!std::all_of(std::begin(fs), std::end(fs), mailbox_empty))
      for (auto& f : fs)
        f.self->mbox.new_round(1, f);
  }

  template <class... Ts>
  void next_cycle(Ts&... xs) {
    entity* es[] = {&xs...};
    CAF_MESSAGE("advance clock by " << tc.credit_interval.count() << "ns");
    tc.global_time += tc.credit_interval;
    for (auto e : es)
      e->advance_time();
  }

  template <class F, class... Ts>
  void loop_until(F pred, Ts&... xs) {
    entity* es[] = {&xs...};
    msg_visitor fs[] = {{&xs}...};
    auto mailbox_empty = [](msg_visitor& x) { return x.self->mbox.empty(); };
    do {
      while (!std::all_of(std::begin(fs), std::end(fs), mailbox_empty))
        for (auto& f : fs)
          f.self->mbox.new_round(1, f);
      CAF_MESSAGE("advance clock by " << tc.step.count() << "ns");
      tc.global_time += tc.step;
      for (auto e : es)
        e->advance_time();
    }
    while (!pred());
  }

  bool done_streaming() {
    entity* es[] = {&alice, &bob, &carl};
    return std::all_of(std::begin(es), std::end(es),
                       [](entity* e) { return e->managers_.empty(); });
  }
};

vector<int> make_iota(int first, int last) {
  vector<int> result(last - first);
  std::iota(result.begin(), result.end(), first);
  return result;
}

} // namespace <anonymous>

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(queue_multiplexing_tests, fixture)

CAF_TEST(depth_2_pipeline_single_round) {
  alice.start_streaming(bob, 30);
  loop(alice, bob);
  next_cycle(alice, bob); // a single credit round is enough
  loop(alice, bob);
  CAF_CHECK_EQUAL(bob.data, make_iota(0, 30));
}

CAF_TEST(depth_2_pipeline_multiple_rounds) {
  constexpr size_t num_messages = 200000;
  alice.start_streaming(bob, num_messages);
  loop_until([&] { return done_streaming(); }, alice, bob);
  CAF_CHECK_EQUAL(bob.data, make_iota(0, num_messages));
}

CAF_TEST(depth_3_pipeline_single_round) {
  bob.forward_to(carl);
  alice.start_streaming(bob, 30);
  loop(alice, bob, carl);
  next_cycle(alice, bob, carl); // a single credit round is enough
  loop(alice, bob, carl);
  CAF_CHECK_EQUAL(bob.data, make_iota(0, 30));
  CAF_CHECK_EQUAL(carl.data, make_iota(0, 30));
}

CAF_TEST(depth_3_pipeline_multiple_rounds) {
  constexpr size_t num_messages = 200000;
  bob.forward_to(carl);
  alice.start_streaming(bob, num_messages);
  CAF_MESSAGE("loop over alice and bob until bob is congested");
  loop(alice, bob);
  CAF_CHECK_NOT_EQUAL(bob.data.size(), 0u);
  CAF_CHECK_EQUAL(carl.data.size(), 0u);
  CAF_MESSAGE("loop over bob and carl until bob finsihed sending");
  // bob has one batch from alice in its mailbox that bob will read when
  // becoming uncongested again
  loop(bob, carl);
  CAF_CHECK_EQUAL(bob.data.size(), carl.data.size());
  CAF_MESSAGE("loop over all until done");
  loop_until([&] { return done_streaming(); }, alice, bob, carl);
  CAF_CHECK_EQUAL(bob.data, make_iota(0, num_messages));
  CAF_CHECK_EQUAL(carl.data, make_iota(0, num_messages));
}

CAF_TEST_FIXTURE_SCOPE_END()
