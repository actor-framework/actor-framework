// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/zip_with.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/op/never.hpp"

#include <limits>
#include <source_location>
#include <utility>

using namespace caf;

namespace {

class recording_sub : public test::fixture::flow::counting_sub {
public:
  recording_sub(flow::coordinator* parent, flow::observer<int> out)
    : counting_sub(parent), out_(std::move(out)) {
    // nop
  }

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t n) override {
    requests.push_back(n);
    if (!out_)
      return;
    test::runnable::current().require(n <= std::numeric_limits<size_t>::max()
                                             - pending);
    pending += n;
  }

  void start() {
    out_.on_subscribe(flow::subscription{intrusive_ptr{this, add_ref}});
  }

  void push(int value,
            const std::source_location& loc = std::source_location::current()) {
    auto& ctx = test::runnable::current();
    ctx.require(out_.valid(), loc);
    ctx.require(pending > 0, loc);
    --pending;
    out_.on_next(value);
  }

  void finish(const error& reason = {}) {
    pending = 0;
    if (reason.valid())
      out_.on_error(reason);
    else
      out_.on_complete();
  }

  std::vector<size_t> requests;
  size_t pending = 0;

private:
  void do_dispose(bool from_external) override {
    pending = 0;
    if (!out_)
      return;
    if (from_external)
      out_.on_error(make_error(sec::disposed));
    else
      out_.release_later();
  }

  flow::observer<int> out_;
};

class recording_source : public flow::op::cold<int> {
public:
  explicit recording_source(flow::coordinator* parent) : cold(parent) {
    // nop
  }

  disposable subscribe(flow::observer<int> out) override {
    sub = parent()->add_child(std::in_place_type<recording_sub>,
                              std::move(out));
    sub->start();
    return sub->as_disposable();
  }

  flow::observable<int> as_observable() {
    return flow::observable<int>{intrusive_ptr{this, add_ref}};
  }

  intrusive_ptr<recording_sub> sub;
};

class recording_observer : public test::fixture::flow::passive_observer<int> {
public:
  using passive_observer::passive_observer;

  void on_complete() override {
    ++completions;
    passive_observer::on_complete();
  }

  void on_error(const error& reason) override {
    ++errors;
    passive_observer::on_error(reason);
  }

  size_t completions = 0;
  size_t errors = 0;
};

struct fixture : test::fixture::flow {
  template <class F, class Out, class... Ts>
  auto make_zip_with_sub(F fn, caf::flow::observer<Out> out, Ts... inputs) {
    using impl_t = caf::flow::op::zip_with_sub<F, typename Ts::output_type...>;
    auto pack = std::make_tuple(std::move(inputs).as_observable()...);
    auto sub = make_counted<impl_t>(coordinator(), std::move(fn), out, pack);
    out.on_subscribe(caf::flow::subscription{sub});
    return sub;
  }

  template <class T>
  auto make_never() {
    auto ptr = make_counted<caf::flow::op::never<T>>(coordinator());
    return caf::flow::observable<T>{std::move(ptr)};
  }

  template <class T>
  auto make_never_sub(caf::flow::observer<T> out) {
    return make_counted<caf::flow::op::never_sub<T>>(coordinator(), out);
  }

  auto make_input() {
    return coordinator()->add_child(std::in_place_type<recording_source>);
  }
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("zip_with ignores requests for zero items") {
  auto first = make_input();
  auto second = make_input();
  auto snk = make_passive_observer<int>();
  first->as_observable()
    .zip_with([](int x, int y) { return x + y; }, second->as_observable())
    .subscribe(snk->as_observer());
  snk->request(0);
  for (auto input : {first, second})
    check(input->sub->requests.empty());
  snk->request(2);
  first->sub->push(1);
  snk->request(0);
  for (auto input : {first, second})
    check_eq(input->sub->requests, std::vector<size_t>{2});
  check_eq(first->sub->pending, 1u);
  check_eq(second->sub->pending, 2u);
  check(snk->buf.empty());
  snk->request(1);
  for (auto input : {first, second})
    check_eq(input->sub->requests, (std::vector<size_t>{2, 1}));
  second->sub->push(10);
  first->sub->push(2);
  second->sub->push(20);
  first->sub->push(3);
  second->sub->push(30);
  check_eq(snk->buf, (std::vector{11, 22, 33}));
  snk->unsubscribe();
}

TEST(
  "zip_with accounts for buffered and in-flight items across uneven inputs") {
  auto first = make_input();
  auto second = make_input();
  using tuple_type = std::tuple<int, int>;
  auto snk = make_passive_observer<tuple_type>();
  first->as_observable()
    .zip_with([](int x, int y) { return std::make_tuple(x, y); },
              second->as_observable())
    .subscribe(snk->as_observer());
  snk->request(2);
  first->sub->push(1);
  check(snk->buf.empty());
  snk->request(1);
  check_eq(first->sub->pending, 2u);
  check_eq(second->sub->pending, 3u);
  // Deliver the remaining items and verify that zip preserves pair order.
  second->sub->push(10);
  first->sub->push(2);
  second->sub->push(20);
  first->sub->push(3);
  second->sub->push(30);
  check_eq(snk->buf, (std::vector<tuple_type>{{1, 10}, {2, 20}, {3, 30}}));
  for (auto input : {first, second})
    check_eq(input->sub->pending, 0u);
  snk->unsubscribe();
}

TEST("zip_with handles demand at the size_t limit") {
  auto first = make_input();
  auto second = make_input();
  auto snk = make_passive_observer<int>();
  first->as_observable()
    .zip_with([](int x, int y) { return x + y; }, second->as_observable())
    .subscribe(snk->as_observer());
  auto max_demand = std::numeric_limits<size_t>::max();
  snk->request(max_demand - 1);
  first->sub->push(1);
  SECTION("demand reaches the limit") {
    snk->request(1);
  }
  SECTION("demand exceeding the limit saturates") {
    snk->request(max_demand);
  }
  for (auto input : {first, second})
    check_eq(input->sub->requests, (std::vector<size_t>{max_demand - 1, 1}));
  check_eq(first->sub->pending, max_demand - 1);
  check_eq(second->sub->pending, max_demand);
  second->sub->push(10);
  snk->request(1);
  for (auto input : {first, second}) {
    check_eq(input->sub->requests, (std::vector<size_t>{max_demand - 1, 1, 1}));
    check_eq(input->sub->pending, max_demand);
  }
  first->sub->push(2);
  second->sub->push(20);
  first->sub->finish();
  check_eq(snk->buf, (std::vector{11, 22}));
  check(snk->completed());
  check(snk->err.empty());
  check(second->sub->disposed());
}

TEST("zip_with does not request more from a terminated input") {
  auto reason = error{};
  auto source_first = true;
  SECTION("the input completes") {
    SECTION("source is the first input") {
      source_first = true;
    }
    SECTION("source is the second input") {
      source_first = false;
    }
  }
  SECTION("the input fails") {
    reason = make_error(sec::runtime_error);
    SECTION("source is the first input") {
      source_first = true;
    }
    SECTION("source is the second input") {
      source_first = false;
    }
  }
  auto source = make_input();
  auto peer = make_input();
  auto snk = coordinator()->add_child(std::in_place_type<recording_observer>);
  auto add = [](int x, int y) { return x + y; };
  auto values = caf::flow::observable<int>{};
  if (source_first)
    values = source->as_observable().zip_with(add, peer->as_observable());
  else
    values = peer->as_observable().zip_with(add, source->as_observable());
  values.subscribe(snk->as_observer());
  snk->request(1);
  source->sub->push(10);
  source->sub->finish(reason);
  check(snk->subscribed());
  check(snk->buf.empty());
  snk->request(1);
  check_eq(source->sub->requests, std::vector<size_t>{1});
  peer->sub->push(1);
  run_flows();
  check_eq(snk->buf, std::vector{11});
  check_eq(snk->completions, reason.valid() ? 0u : 1u);
  check_eq(snk->errors, reason.valid() ? 1u : 0u);
  check_eq(snk->err, reason);
  check(peer->sub->disposed());
}

TEST("zip_with cancels sources with unused credit and ignores later requests") {
  auto first = make_input();
  auto second = make_input();
  auto snk = make_passive_observer<int>();
  first->as_observable()
    .zip_with([](int x, int y) { return x + y; }, second->as_observable())
    .subscribe(snk->as_observer());
  snk->request(2);
  first->sub->push(1);
  auto sub = snk->sub;
  snk->unsubscribe();
  sub.request(3);
  run_flows();
  for (auto input : {first, second}) {
    check(input->sub->disposed());
    check_eq(input->sub->pending, 0u);
    check_eq(input->sub->requests, std::vector<size_t>{2});
  }
  check(snk->buf.empty());
  check(snk->idle());
}

SCENARIO("zip_with combines inputs") {
  GIVEN("two observables") {
    WHEN("merging them with zip_with") {
      THEN("the observer receives the combined output of both sources") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .zip_with([](int x, int y) { return x + y; },
                    make_observable().repeat(11).take(113),
                    make_observable().repeat(22).take(223))
          .subscribe(snk->as_observer());
        run_flows();
        require_eq(snk->state, flow::observer_state::subscribed);
        snk->sub.request(64);
        run_flows();
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf.size(), 64u);
        snk->sub.request(64);
        run_flows();
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->buf.size(), 113u);
        check_eq(snk->buf, std::vector<int>(113, 33));
      }
    }
  }
}

SCENARIO("zip_with emits nothing when zipping an empty observable") {
  GIVEN("two observables, one of them empty") {
    WHEN("merging them with zip_with") {
      THEN("the observer sees on_complete immediately") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .zip_with([](int x, int y, int z) { return x + y + z; },
                    coordinator()->make_observable().repeat(11),
                    coordinator()->make_observable().repeat(22),
                    coordinator()->make_observable().empty<int>())
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->buf.empty());
        check_eq(snk->state, flow::observer_state::completed);
      }
    }
  }
}

SCENARIO("zip_with aborts if an input emits an error") {
  GIVEN("two observables, one of them emits an error after some items") {
    WHEN("merging them with zip_with") {
      THEN("the observer receives all items up to the error") {
        using snk_t = flow::auto_observer<int>;
        auto obs = make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        obs //
          .iota(1)
          .take(3)
          .concat(obs.fail<int>(sec::runtime_error))
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1).take(10))
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check_eq(snk->buf, std::vector<int>({2, 4, 6}));
      }
    }
  }
  GIVEN("two observables, one of them emits an error immediately") {
    WHEN("merging them with zip_with") {
      THEN("the observer only receives on_error") {
        using snk_t = flow::auto_observer<int>;
        auto obs = coordinator()->make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        obs //
          .iota(1)
          .take(3)
          .zip_with([](int x, int y) { return x + y; },
                    obs.fail<int>(sec::runtime_error))
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
      }
    }
  }
}

SCENARIO("zip_with on an invalid observable produces an invalid observable") {
  GIVEN("a default-constructed (invalid) observable") {
    WHEN("calling zip_with on it") {
      THEN("the result is another invalid observable") {
        using snk_t = flow::auto_observer<int>;
        auto obs = coordinator()->make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        caf::flow::observable<int>{}
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1).take(10))
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
      }
    }
  }
  GIVEN("a valid observable") {
    WHEN("calling zip_with on it with an invalid observable") {
      THEN("the result is another invalid observable") {
        using snk_t = flow::auto_observer<int>;
        auto obs = coordinator()->make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        obs //
          .iota(1)
          .take(10)
          .zip_with([](int x, int y) { return x + y; },
                    caf::flow::observable<int>{})
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
      }
    }
  }
}

SCENARIO("zip_with operators can be disposed at any time") {
  auto obs = coordinator()->make_observable();
  GIVEN("a zip_with operator that produces some items") {
    WHEN("calling dispose before requesting any items") {
      THEN("the observer never receives any item") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto sub = //
          obs      //
            .iota(1)
            .take(10)
            .zip_with([](int x, int y) { return x + y; }, obs.iota(1))
            .subscribe(snk->as_observer());
        check(!sub.disposed());
        sub.dispose();
        run_flows();
        check(snk->aborted());
      }
    }
    WHEN("calling dispose in on_subscribe") {
      THEN("the observer receives no item") {
        using snk_t = flow::canceling_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>, false);
        obs //
          .iota(1)
          .take(10)
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1))
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(snk->on_next_calls, 0);
      }
    }
    WHEN("calling dispose in on_next") {
      THEN("the observer receives no additional item") {
        using snk_t = flow::canceling_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>, true);
        obs //
          .iota(1)
          .take(10)
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1))
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(snk->on_next_calls, 1);
      }
    }
  }
}

SCENARIO("the zip_with operators disposes unexpected subscriptions") {
  GIVEN("a zip_with operator with two inputs") {
    WHEN("on_subscribe is called twice for the same input") {
      THEN("the operator disposes the subscription") {
        using caf::flow::op::zip_index;
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_zip_with_sub([](int, int) { return 0; },
                                     snk->as_observer(), make_never<int>(),
                                     make_never<int>());
        auto sub1 = make_never_sub<int>(snk->as_observer());
        auto sub2 = make_never_sub<int>(snk->as_observer());
        uut->fwd_on_subscribe(zip_index<0>{}, caf::flow::subscription{sub1});
        uut->fwd_on_subscribe(zip_index<0>{}, caf::flow::subscription{sub2});
        check(sub1->disposed());
        check(sub2->disposed());
        uut->dispose();
        run_flows();
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
