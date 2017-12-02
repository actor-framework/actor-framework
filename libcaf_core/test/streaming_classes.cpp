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

#define CAF_SUITE streaming_classes

#include <memory>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/downstream_msg.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/no_stages.hpp"
#include "caf/send.hpp"
#include "caf/stream_slot.hpp"
#include "caf/system_messages.hpp"
#include "caf/upstream_msg.hpp"
#include "caf/variant.hpp"

#include "caf/mixin/sender.hpp"

#include "caf/test/unit_test.hpp"

#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/singly_linked.hpp"
#include "caf/intrusive/task_result.hpp"
#include "caf/intrusive/wdrr_dynamic_multiplexed_queue.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"

#include "caf/detail/overload.hpp"

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

// -- forward declarations -----------------------------------------------------

// Mimics an actor.
class entity;

// Mimics a stream_manager.
struct manager;

// Mimics an inbound_path.
struct in;

// Mimics an outbound_path.
struct out;

// -- manager and path handlers ------------------------------------------------

struct manager {
  entity* self;
  int x;
  int num_messages;

  int input_paths = 0;
  int output_paths = 0;

  manager(entity* parent, int num = 0) : self(parent), x(0), num_messages(num) {
    // nop
  }

  bool done() const {
    return (input_paths | output_paths) == 0;
  }

  void push(stream_slots slots, strong_actor_ptr& to, int num);

  void operator()(stream_slots slots, in* path, downstream_msg::batch& x);
};

using manager_ptr = std::shared_ptr<manager>;

struct in {
  manager_ptr mgr;
  strong_actor_ptr hdl;
  in(manager_ptr ptr, strong_actor_ptr src)
      : mgr(std::move(ptr)),
        hdl(std::move(src)) {
    // nop
  }

  void operator()(mailbox_element& x);
};

struct out {

};

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
  manager* mgr;
  umsg_queue_policy(manager* ptr) : mgr(ptr) {
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
    CAF_REQUIRE_NOT_EQUAL(x.xs_size, 0);
    return static_cast<task_size_type>(x.xs_size);
  }

  template <class T>
  task_size_type operator()(const T&) const {
    return 1;
  }

  inner_dmsg_queue_policy(std::unique_ptr<in> ptr) : handler(std::move(ptr)) {
    // nop
  }

  std::unique_ptr<in> handler;
};

using inner_dmsg_queue = drr_queue<inner_dmsg_queue_policy>;

struct dmsg_queue_policy : policy_base {
  using key_type = stream_slot;

  using queue_map_type = std::map<stream_slot, inner_dmsg_queue>;

  key_type id_of(mailbox_element& x) {
    return x.content().get_as<downstream_msg>(0).slots.receiver;
  }

  template <class Queue>
  deficit_type quantum(const Queue&, deficit_type x) {
    return x;
  }
};

using dmsg_queue = wdrr_dynamic_multiplexed_queue<dmsg_queue_policy>;

struct mbox_policy : policy_base {
  template <class Queue>
  deficit_type quantum(const Queue&, deficit_type x) {
    return x;
  }

  size_t id_of(const mailbox_element& x) {
    return x.mid.category();
  }
};

using mbox_queue = wdrr_fixed_multiplexed_queue<mbox_policy, default_queue,
                                                umsg_queue, dmsg_queue,
                                                default_queue>;

// -- manager and entity -------------------------------------------------------

template <class Target>
struct dispatcher {
  Target& f;
  entity* sender;
  stream_slots slots;

  template <class T>
  void operator()(T&& x) {
    f(sender, slots, std::forward<T>(x));
  }
};

class entity : public extend<abstract_actor, entity>::with<mixin::sender> {
public:
  // -- member types -----------------------------------------------------------

  using super = extend<abstract_actor, entity>::with<mixin::sender>;

  using signatures = none_t;

  using behavior_type = behavior;

  entity(actor_config& cfg, const char* cstr_name)
      : super(cfg),
        mbox_(mbox_policy{}, default_queue_policy{}, nullptr,
              dmsg_queue_policy{}, default_queue_policy{}),
        name_(cstr_name) {
    // nop
  }

  static const char* name_of(const strong_actor_ptr& x) {
    CAF_ASSERT(x != nullptr);
    auto buddy = dynamic_cast<entity*>(x->get());
    CAF_ASSERT(buddy != nullptr);
    return buddy->name();
  }

  static const char* name_of(const actor_addr& x) {
    return name_of(actor_cast<strong_actor_ptr>(x));
  }

  void enqueue(mailbox_element_ptr what, execution_unit*) override {
    auto push_back_result = mbox_.push_back(std::move(what));
    CAF_CHECK_EQUAL(push_back_result, true);
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

  const char* name() const {
    return name_;
  }

  execution_unit* context() {
    return nullptr;
  }

  void start_streaming(strong_actor_ptr to, int num_messages) {
    CAF_REQUIRE_NOT_EQUAL(to, nullptr);
    CAF_REQUIRE_NOT_EQUAL(num_messages, 0);
    auto slot = next_slot_++;
    CAF_MESSAGE(name_ << " starts streaming to " << name_of(to)
                << " on slot " << slot);
    send(to, open_stream_msg{slot, make_message(), ctrl(), nullptr,
                             stream_priority::normal, false});
    auto ptr = std::make_shared<manager>(this, num_messages);
    ptr->output_paths += 1;
    pending_managers_.emplace(slot, std::move(ptr));
  }

  void operator()(open_stream_msg& hs) {
    TRACE(name_, stream_handshake_msg,
          CAF_ARG2("sender", name_of(hs.prev_stage)));
    auto slot = next_slot_++;
    //stream_slots id{slot, hs.sender_slot};
    stream_slots id{hs.slot, slot};
    // Create required state.
    auto mgr = std::make_shared<manager>(this, 0);
    managers_.emplace(id, mgr);
    mgr->input_paths += 1;
    // Create a new queue in the mailbox for incoming traffic.
    get<2>(mbox_.queues())
      .queues()
      .emplace(slot, std::unique_ptr<in>{new in(mgr, hs.prev_stage)});
    // Acknowledge stream.
    send(hs.prev_stage,
         make<upstream_msg::ack_open>(id.invert(), address(), address(),
                                      ctrl(), 10, 10, false));
  }

  void operator()(stream_slots slots, actor_addr& sender,
                  upstream_msg::ack_open& x) {
    TRACE(name_, ack_handshake, CAF_ARG(slots),
          CAF_ARG2("sender", name_of(x.rebind_to)));
    // Get the manager for that stream.
    auto i = pending_managers_.find(slots.receiver);
    CAF_REQUIRE_NOT_EQUAL(i, pending_managers_.end());
    // Create a new queue in the mailbox for incoming traffic.
    // Swap the buddy/receiver perspective to generate the ID we are using.
    managers_.emplace(slots, i->second);
    auto to = actor_cast<strong_actor_ptr>(sender);
    CAF_REQUIRE_NOT_EQUAL(to, nullptr);
    i->second->push(slots.invert(), to, x.initial_demand);
    pending_managers_.erase(i);
  }

  void operator()(stream_slots input_slots, actor_addr& sender,
                  upstream_msg::ack_batch& x) {
    TRACE(name_, ack_batch, CAF_ARG(input_slots),
          CAF_ARG2("sender", name_of(sender)));
    // Get the manager for that stream.
    auto slots = input_slots.invert();
    auto i = managers_.find(input_slots);
    CAF_REQUIRE_NOT_EQUAL(i, managers_.end());
    auto to = actor_cast<strong_actor_ptr>(sender);
    CAF_REQUIRE_NOT_EQUAL(to, nullptr);
    i->second->push(slots, to, x.new_capacity);
    if (i->second->done())
      managers_.erase(i);
  }

  void operator()(stream_slots slots, in*, downstream_msg::close&) {
    //TRACE(name, close, CAF_ARG(slots), CAF_ARG2("sender", buddy->name));
    TRACE(name_, close, CAF_ARG(slots));
    auto i = managers_.find(slots);
    CAF_REQUIRE_NOT_EQUAL(i, managers_.end());
    i->second->input_paths -= 1;
    get<2>(mbox_.queues()).erase_later(slots.receiver);
    if (i->second->done()) {
      CAF_MESSAGE(name_ << " cleans up path " << deep_to_string(slots));
      managers_.erase(i);
    }
  }

  void next_slot(stream_slot x) {
    next_slot_ = x;
  }

  mbox_queue& mbox() {
    return mbox_;
  }

  // -- member variables -------------------------------------------------------

  mbox_queue mbox_;
  const char* name_;
  stream_slot next_slot_ = 1;
  std::map<stream_slot, manager_ptr> pending_managers_;
  std::map<stream_slots, manager_ptr> managers_;
};

void manager::push(stream_slots slots, strong_actor_ptr& to, int num) {
  CAF_REQUIRE_NOT_EQUAL(num, 0);
  CAF_REQUIRE_NOT_EQUAL(to, nullptr);
  std::vector<int> xs;
  if (x + num > num_messages)
    num = num_messages - x;
  if (num == 0) {
    CAF_MESSAGE(self->name() << " is done sending batches");
    anon_send(to, make<downstream_msg::close>(slots, nullptr));
    output_paths -= 1;
    return;
  }
  CAF_MESSAGE(self->name() << " pushes "
              << num << " new items to " << entity::name_of(to)
              << " slots = " << deep_to_string(slots));
  for (int i = 0; i < num; ++i)
    xs.emplace_back(x++);
  CAF_REQUIRE_NOT_EQUAL(xs.size(), 0u);
  auto xss = static_cast<int32_t>(xs.size());
  anon_send(to, make<downstream_msg::batch>(slots, nullptr, xss,
                                            make_message(std::move(xs)), 0));
}

void manager::operator()(stream_slots slots, in* i,
                         downstream_msg::batch& batch) {
  TRACE(self->name(), batch, CAF_ARG(slots),
        CAF_ARG2("sender", entity::name_of(i->hdl)), CAF_ARG(batch.xs));
  anon_send(i->hdl, make<upstream_msg::ack_batch>(slots.invert(),
                                                  self->address(), 10, 10, 0));
}

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
  result_type operator()(is_dmsg, dmsg_queue&, stream_slot,
                         inner_dmsg_queue& q, mailbox_element& x) {
    CAF_REQUIRE(x.content().type_token() == make_type_token<downstream_msg>());
    auto inptr = q.policy().handler.get();
    auto& dm = x.content().get_mutable_as<downstream_msg>(0);
    auto f = detail::make_overload(
      [&](downstream_msg::batch& y) {
        (*inptr->mgr)(dm.slots, inptr, y);
      },
      [&](downstream_msg::close& y) {
        auto self = inptr->mgr->self;
        (*self)(dm.slots, inptr, y);
      },
      [](downstream_msg::forced_close&) {
        CAF_FAIL("did not expect downstream_msg::forced_close");
      }
    );
    visit(f, dm.content);
    return intrusive::task_result::resume;
  }

  // -- member variables -------------------------------------------------------

  entity* self;
};

// -- fixture ------------------------------------------------------------------

struct fixture {
  actor_system_config cfg;
  actor_system sys{cfg};
  actor alice_hdl;
  actor bob_hdl;

  entity* alice;
  entity* bob;

  fixture() {
    actor_config ac1;
    alice_hdl = make_actor<entity>(0, node_id{}, &sys, ac1, "alice");
    actor_config ac2;
    bob_hdl = make_actor<entity>(1, node_id{}, &sys, ac2, "bob");
    /// Make sure to test whether the slot IDs are properly handled.
    alice = static_cast<entity*>(actor_cast<abstract_actor*>(alice_hdl));
    bob = static_cast<entity*>(actor_cast<abstract_actor*>(bob_hdl));
    alice->next_slot(123);
    bob->next_slot(321);
  }
};

} // namespace <anonymous>

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(queue_multiplexing_tests, fixture)

CAF_TEST(simple_handshake) {
  bob->start_streaming(actor_cast<strong_actor_ptr>(alice_hdl), 30);
  msg_visitor f{alice};
  msg_visitor g{bob};
  while (!alice->mbox_.empty() || !bob->mbox_.empty()) {
    alice->mbox_.new_round(1, f);
    bob->mbox_.new_round(1, g);
  }
  // Check whether alice and bob cleaned up their state properly.
  CAF_CHECK(get<2>(alice->mbox_.queues()).queues().empty());
  CAF_CHECK(get<2>(bob->mbox_.queues()).queues().empty());
  CAF_CHECK(alice->pending_managers_.empty());
  CAF_CHECK(bob->pending_managers_.empty());
  CAF_CHECK(alice->managers_.empty());
  CAF_CHECK(bob->managers_.empty());
}

CAF_TEST_FIXTURE_SCOPE_END()
