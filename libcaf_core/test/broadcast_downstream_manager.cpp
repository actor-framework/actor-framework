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

#define CAF_SUITE broadcast_downstream_manager

#include <cstdint>
#include <memory>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/broadcast_downstream_manager.hpp"
#include "caf/scheduled_actor.hpp"

#include "caf/mixin/sender.hpp"

#include "caf/test/unit_test.hpp"

using namespace caf;

namespace {

using bcast_manager = broadcast_downstream_manager<int>;

// Mocks just enough of a stream manager to serve our entity.
class mock_stream_manager : public stream_manager {
public:
  using super = stream_manager;

  mock_stream_manager(scheduled_actor* self) : super(self), out_(this) {
    // nop
  }

  bcast_manager& out() override {
    return out_;
  }

  bool done() const override {
    return false;
  }

  bool idle() const noexcept override {
    return false;
  }

private:
  bcast_manager out_;
};

// Mocks just enough of an actor to receive and send batches.
class entity : public scheduled_actor {
public:
  // -- member types -----------------------------------------------------------

  using super = scheduled_actor;

  using signatures = none_t;

  using behavior_type = behavior;

  entity(actor_config& cfg, const char* cstr)
      : super(cfg),
        cstr_name(cstr),
        mgr(this),
        next_slot(1) {
    // nop
  }

  void enqueue(mailbox_element_ptr what, execution_unit*) override {
    mbox.push_back(what->move_content_to_message());
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

  void launch(execution_unit*, bool, bool) override {
    // nop
  }

  void add_path_to(entity& x, int32_t desired_batch_size) {
    auto ptr = mgr.out().add_path(next_slot++, x.ctrl());
    CAF_REQUIRE(ptr != nullptr);
    ptr->set_desired_batch_size(desired_batch_size);
    ptr->slots.receiver = x.next_slot++;
    paths.emplace_back(ptr);
  }

  size_t credit_for(entity& x) {
    auto pred = [&](outbound_path* ptr) {
      return ptr->hdl == &x;
    };
    auto i = std::find_if(paths.begin(), paths.end(), pred);
    CAF_REQUIRE(i != paths.end());
    return static_cast<size_t>((*i)->open_credit);
  }

  void new_round(int num, bool force_emit) {
    for (auto& ptr : paths)
      ptr->open_credit += num;
    if (force_emit)
      mgr.out().force_emit_batches();
    else
      mgr.out().emit_batches();
  }

  const char* name() const override {
    return cstr_name;
  }

  // -- member variables -------------------------------------------------------

  const char* cstr_name;

  /// Manager-under-test.
  mock_stream_manager mgr;

  /// Dummy mailbox.
  std::vector<message> mbox;

  /// All outbound paths managed by `bs`.
  std::vector<outbound_path*> paths;

  /// Next free ID.
  stream_slot next_slot;
};

// Needed by `some` macro.
struct not_empty_t {};

constexpr auto some = not_empty_t{};

// Provides the setup with alice, bob, and carl.
struct fixture {
  actor_system_config cfg;

  actor_system sys;

  strong_actor_ptr alice_hdl;

  strong_actor_ptr bob_hdl;

  strong_actor_ptr carl_hdl;

  entity& alice;

  entity& bob;

  entity& carl;

  static strong_actor_ptr spawn(actor_system& sys, actor_id id,
                                const char* name) {
    actor_config conf;
    auto hdl = make_actor<entity>(id, node_id{}, &sys, conf, name);
    return actor_cast<strong_actor_ptr>(std::move(hdl));
  }

  static entity& fetch(const strong_actor_ptr& hdl) {
    return *static_cast<entity*>(actor_cast<abstract_actor*>(hdl));
  }

  fixture()
      : sys(cfg),
        alice_hdl(spawn(sys, 0, "alice")),
        bob_hdl(spawn(sys, 1, "bob")),
        carl_hdl(spawn(sys, 2, "carl")),
        alice(fetch(alice_hdl)),
        bob(fetch(bob_hdl)),
        carl(fetch(carl_hdl)) {
    // nop
  }

  using batch_type = std::vector<int>;

  using batches_type = std::vector<batch_type>;

  batches_type batches(entity& x) {
    auto& mbox = x.mbox;
    std::vector<batch_type> result;
    for (auto& msg : mbox) {
      CAF_REQUIRE(msg.match_elements<downstream_msg>());
      auto& dm = msg.get_mutable_as<downstream_msg>(0);
      CAF_REQUIRE(holds_alternative<downstream_msg::batch>(dm.content));
      auto& b = get<downstream_msg::batch>(dm.content);
      CAF_REQUIRE(b.xs.match_elements<batch_type>());
      result.emplace_back(std::move(b.xs.get_mutable_as<batch_type>(0)));
    }
    mbox.clear();
    return result;
  }

  batch_type make_batch(int first, int last) {
    batch_type result;
    result.resize(static_cast<size_t>((last + 1) - first));
    std::iota(result.begin(), result.end(), first);
    return result;
  }
};

// Enables `received` macro.
template <class F>
struct receive_checker {
  receive_checker(F fun) : f(fun), moved_from(false), check_not_empty(false) {
    // nop
  }

  receive_checker(receive_checker&& x)
      : f(x.f),
        xs(std::move(x.xs)),
        moved_from(false),
        check_not_empty(x.check_not_empty) {
    x.moved_from = true;
  }

  ~receive_checker() {
    if (!moved_from)
      f(xs, check_not_empty);
  }

  F f;
  fixture::batches_type xs;
  bool moved_from;
  bool check_not_empty;
};

template <class F>
receive_checker<F> make_receive_checker(F fun) {
  return {fun};
}

template <class F>
receive_checker<F> operator<<(receive_checker<F> xs, fixture::batch_type ys) {
  xs.xs.emplace_back(std::move(ys));
  return xs;
}

template <class F>
receive_checker<F> operator<<(receive_checker<F> xs, none_t) {
  return xs;
}

template <class F>
receive_checker<F> operator<<(receive_checker<F> xs, not_empty_t) {
  xs.check_not_empty = true;
  return xs;
}

} // namespace <anonymous>

// -- DSL for near-natural-language testing ------------------------------------

#define CONCAT_2(x, y) x##y

#define CONCAT(x, y) CONCAT_2(x, y)

#define RECEIVED                                                               \
  ;                                                                            \
  make_receive_checker([&](fixture::batches_type& xs, bool check_not_empty) {  \
    if (!check_not_empty)                                                      \
      CAF_CHECK_EQUAL(batches(CONCAT(who, __LINE__)), xs);                     \
    else                                                                       \
      CAF_CHECK(!batches(CONCAT(who, __LINE__)).empty());                      \
  }) <<

#define ENTITY auto& CONCAT(who, __LINE__) =

#define AFTER

#define HAS ; size_t CONCAT(amount, __LINE__) =

#define CREDIT ;

#define TRIED ;

#define FORCE_SENDING                                                          \
  auto CONCAT(force, __LINE__) = true;                                         \
  auto CONCAT(amount, __LINE__) =

#define SENDING                                                                \
  auto CONCAT(force, __LINE__) = false;                                        \
  auto CONCAT(amount, __LINE__) =

#define ELEMENTS                                                               \
  ;                                                                            \
  CONCAT(who, __LINE__)                                                        \
    .new_round(CONCAT(amount, __LINE__), CONCAT(force, __LINE__));             \
  CAF_MESSAGE(CONCAT(who, __LINE__).name()                                     \
              << " tried sending " << CONCAT(amount, __LINE__)                 \
              << " elements");

#define FOR                                                                    \
  struct CONCAT(for_helper_, __LINE__) {                                       \
    entity& who;                                                               \
    size_t amount;                                                             \
    void operator<<(entity& x) const {                                         \
      CAF_CHECK_EQUAL(who.credit_for(x), amount);                              \
    }                                                                          \
  };                                                                           \
  CONCAT(for_helper_, __LINE__){CONCAT(who, __LINE__),                         \
                                CONCAT(amount, __LINE__)}                      \
    <<

#define TOTAL                                                                  \
  CAF_CHECK_EQUAL(CONCAT(who, __LINE__).mgr.out().total_credit(),              \
                  CONCAT(amount, __LINE__))

#define BATCH(first, last) make_batch(first, last)

#define AND <<

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(broadcast_downstream_manager, fixture)

CAF_TEST(one_path_force) {
  // Give alice 100 elements to send and a path to bob with desired batch size
  // of 10.
  alice.add_path_to(bob, 10);
  for (int i = 1; i <= 100; ++i)
    alice.mgr.out().push(i);
  // Give 3 credit (less than 10).
  AFTER ENTITY alice TRIED FORCE_SENDING 3 ELEMENTS {
    ENTITY bob RECEIVED BATCH(1, 3);
    ENTITY alice HAS 0u CREDIT FOR bob;
    ENTITY alice HAS 0u CREDIT TOTAL;
  }
  // Give exactly 10 credit.
  AFTER ENTITY alice TRIED FORCE_SENDING 10 ELEMENTS {
    ENTITY bob RECEIVED BATCH(4, 13);
    ENTITY alice HAS 0u CREDIT FOR bob;
  }
  // Give 11 credit (more than 10).
  AFTER ENTITY alice TRIED FORCE_SENDING 11 ELEMENTS {
    ENTITY bob RECEIVED BATCH(14, 23) AND BATCH(24, 24);
    ENTITY alice HAS 0u CREDIT FOR bob;
  }
  // Drain all elements except the last 5.
  AFTER ENTITY alice TRIED FORCE_SENDING 71 ELEMENTS {
    ENTITY bob RECEIVED some;
    ENTITY alice HAS 0u CREDIT FOR bob;
  }
  // Give more credit than there are elements left.
  AFTER ENTITY alice TRIED FORCE_SENDING 100 ELEMENTS {
    ENTITY bob RECEIVED BATCH(96, 100);
    ENTITY alice HAS 95u CREDIT FOR bob;
  }
  // Give more credit than there are elements left.
  AFTER ENTITY alice TRIED FORCE_SENDING 5 ELEMENTS {
    ENTITY bob RECEIVED none;
    ENTITY alice HAS 100u CREDIT FOR bob;
  }
}

CAF_TEST(one_path_without_force) {
  // Give alice 100 elements to send and a path to bob with desired batch size
  // of 10.
  alice.add_path_to(bob, 10);
  for (int i = 1; i <= 100; ++i)
    alice.mgr.out().push(i);
  // Give 3 credit (less than 10).
  AFTER ENTITY alice TRIED SENDING 3 ELEMENTS {
    ENTITY bob RECEIVED none;
    ENTITY alice HAS 3u CREDIT FOR bob;
    ENTITY alice HAS 3u CREDIT TOTAL;
  }
  // Give exactly 10 credit.
  AFTER ENTITY alice TRIED SENDING 10 ELEMENTS {
    ENTITY bob RECEIVED BATCH(1, 10);
    ENTITY alice HAS 3u CREDIT FOR bob;
  }
  // Give 11 credit (more than 10).
  AFTER ENTITY alice TRIED SENDING 11 ELEMENTS {
    ENTITY bob RECEIVED BATCH(11, 20);
    ENTITY alice HAS 4u CREDIT FOR bob;
  }
  // Drain all elements except the last 5.
  AFTER ENTITY alice TRIED SENDING 71 ELEMENTS {
    ENTITY bob RECEIVED some;
    ENTITY alice HAS 5u CREDIT FOR bob;
  }
  // Give more credit than there are elements left.
  AFTER ENTITY alice TRIED SENDING 100 ELEMENTS {
    ENTITY bob RECEIVED BATCH(91, 100);
    ENTITY alice HAS 95u CREDIT FOR bob;
  }
  // Give more credit than there are elements left.
  AFTER ENTITY alice TRIED SENDING 5 ELEMENTS {
    ENTITY bob RECEIVED none;
    ENTITY alice HAS 100u CREDIT FOR bob;
  }
}

CAF_TEST(two_paths_different_sizes_force) {
  // Give alice 100 elements to send, a path to bob with desired batch size of
  // 10, and a path to carl with desired batch size of 7.
  alice.add_path_to(bob, 10);
  alice.add_path_to(carl, 7);
  for (int i = 1; i <= 100; ++i)
    alice.mgr.out().push(i);
  // Give 3 credit (less than 10).
  AFTER ENTITY alice TRIED FORCE_SENDING 3 ELEMENTS {
    ENTITY bob RECEIVED BATCH(1, 3);
    ENTITY carl RECEIVED BATCH(1, 3);
    ENTITY alice HAS 0u CREDIT FOR bob;
    ENTITY alice HAS 0u CREDIT FOR carl;
    ENTITY alice HAS 0u CREDIT TOTAL;
  }
  // Give exactly 10 credit.
  AFTER ENTITY alice TRIED FORCE_SENDING 10 ELEMENTS {
    ENTITY bob RECEIVED BATCH(4, 13);
    ENTITY carl RECEIVED BATCH(4, 10) AND BATCH(11, 13);
    ENTITY alice HAS 0u CREDIT FOR bob;
    ENTITY alice HAS 0u CREDIT FOR carl;
    ENTITY alice HAS 0u CREDIT TOTAL;
  }
  // Give 11 credit (more than 10).
  AFTER ENTITY alice TRIED FORCE_SENDING 11 ELEMENTS {
    ENTITY bob RECEIVED BATCH(14, 23) AND BATCH(24, 24);
    ENTITY carl RECEIVED BATCH(14, 20) AND BATCH(21, 24);
    ENTITY alice HAS 0u CREDIT TOTAL;
  }
  // Drain all elements except the last 5.
  AFTER ENTITY alice TRIED FORCE_SENDING 71 ELEMENTS {
    ENTITY bob RECEIVED some;
    ENTITY carl RECEIVED some;
    ENTITY alice HAS 0u CREDIT TOTAL;
  }
  // Give more credit than there are elements left.
  AFTER ENTITY alice TRIED FORCE_SENDING 100 ELEMENTS {
    ENTITY bob RECEIVED BATCH(96, 100);
    ENTITY carl RECEIVED BATCH(96, 100);
    ENTITY alice HAS 95u CREDIT FOR bob;
    ENTITY alice HAS 95u CREDIT FOR carl;
    ENTITY alice HAS 190u CREDIT TOTAL;
  }
  // Give more credit than there are elements left.
  AFTER ENTITY alice TRIED FORCE_SENDING 5 ELEMENTS {
    ENTITY bob RECEIVED none;
    ENTITY carl RECEIVED none;
    ENTITY alice HAS 200u CREDIT TOTAL;
  }
}

CAF_TEST(two_paths_different_sizes_without_force) {
  // Give alice 100 elements to send, a path to bob with desired batch size of
  // 10, and a path to carl with desired batch size of 7.
  alice.add_path_to(bob, 10);
  alice.add_path_to(carl, 7);
  for (int i = 1; i <= 100; ++i)
    alice.mgr.out().push(i);
  // Give 3 credit (less than 10).
  AFTER ENTITY alice TRIED SENDING 3 ELEMENTS {
    ENTITY bob RECEIVED none;
    ENTITY carl RECEIVED none;
    ENTITY alice HAS 3u CREDIT FOR bob;
    ENTITY alice HAS 3u CREDIT FOR carl;
    ENTITY alice HAS 6u CREDIT TOTAL;
  }
  // Give 4 more credit.
  AFTER ENTITY alice TRIED SENDING 4 ELEMENTS {
    ENTITY bob RECEIVED none;
    ENTITY carl RECEIVED BATCH(1, 7);
    ENTITY alice HAS 7u CREDIT FOR bob;
    ENTITY alice HAS 0u CREDIT FOR carl;
    ENTITY alice HAS 7u CREDIT TOTAL;
  }
  // Give 3 more credit (reaching 10 for bob).
  AFTER ENTITY alice TRIED SENDING 3 ELEMENTS {
    ENTITY bob RECEIVED BATCH(1, 10);
    ENTITY carl RECEIVED none;
    ENTITY alice HAS 0u CREDIT FOR bob;
    ENTITY alice HAS 3u CREDIT FOR carl;
    ENTITY alice HAS 3u CREDIT TOTAL;
  }
  // Give 11 credit.
  AFTER ENTITY alice TRIED SENDING 11 ELEMENTS {
    ENTITY bob RECEIVED BATCH(11, 20);
    ENTITY carl RECEIVED BATCH(8, 14) AND BATCH(15, 21);
    ENTITY alice HAS 1u CREDIT FOR bob;
    ENTITY alice HAS 0u CREDIT FOR carl;
    ENTITY alice HAS 1u CREDIT TOTAL;
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
