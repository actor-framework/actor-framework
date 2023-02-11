// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.op.concat

#include "caf/flow/op/concat.hpp"

#include "core-test.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

// Like op::empty, but calls on_complete immediately instead of waiting for the
// observer to request items. We use this to get more coverage on edge cases.
template <class T>
class insta_empty : public flow::op::cold<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = flow::op::cold<T>;

  using output_type = T;

  // -- constructors, destructors, and assignment operators --------------------

  explicit insta_empty(flow::coordinator* ctx) : super(ctx) {
    // nop
  }

  // -- implementation of observable<T>::impl ----------------------------------

  disposable subscribe(flow::observer<output_type> out) override {
    auto sub = make_counted<flow::passive_subscription_impl>();
    out.on_subscribe(flow::subscription{sub});
    out.on_complete();
    return sub->as_disposable();
  }
};

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  // Creates a flow::op::concat<T>
  template <class T, class... Inputs>
  auto make_operator(Inputs&&... inputs) {
    return make_counted<flow::op::concat<T>>(ctx.get(),
                                             std::forward<Inputs>(inputs)...);
  }

  // Similar to concat::subscribe, but returns a concat_sub pointer instead of
  // type-erasing it into a disposable.
  template <class T, class... Ts>
  auto raw_sub(flow::observer<T> out, Ts&&... xs) {
    using flow::observable;
    using input_type = std::variant<observable<T>, observable<observable<T>>>;
    auto vec = std::vector<input_type>{std::forward<Ts>(xs).as_observable()...};
    auto ptr = make_counted<flow::op::concat_sub<T>>(ctx.get(), out, vec);
    out.on_subscribe(flow::subscription{ptr});
    return ptr;
  }

  template <class T>
  auto make_insta_empty() {
    return flow::observable<T>{make_counted<insta_empty<T>>(ctx.get())};
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("concat operators combine inputs") {
  GIVEN("two observables that emit values") {
    WHEN("concatenating them to a single publisher") {
      THEN("the observer receives the output of both sources in order") {
        auto outputs = std::vector<int>{};
        auto r1 = ctx->make_observable().repeat(11).take(113);
        auto r2 = ctx->make_observable().repeat(22).take(223);
        auto sub = ctx->make_observable()
                     .concat(std::move(r1), std::move(r2))
                     .for_each([&outputs](int x) { outputs.emplace_back(x); });
        CHECK(!sub.disposed());
        ctx->run();
        if (CHECK_EQ(outputs.size(), 336u)) {
          CHECK(std::all_of(outputs.begin(), outputs.begin() + 113,
                            [](int x) { return x == 11; }));
          CHECK(std::all_of(outputs.begin() + 113, outputs.end(),
                            [](int x) { return x == 22; }));
        }
      }
    }
    WHEN("concatenating them but disposing the operator") {
      THEN("the observer only an on_complete event") {
        auto r1 = ctx->make_observable().repeat(11).take(113);
        auto r2 = ctx->make_observable().repeat(22).take(223);
        auto snk = flow::make_passive_observer<int>();
        auto sub = ctx->make_observable()
                     .concat(std::move(r1), std::move(r2))
                     .subscribe(snk->as_observer());
        ctx->run();
        sub.dispose();
        ctx->run();
        CHECK(snk->completed());
        CHECK(snk->buf.empty());
      }
    }
  }
  GIVEN("an observable of observable") {
    WHEN("concatenating it") {
      THEN("the observer receives all items") {
        auto snk = flow::make_auto_observer<int>();
        ctx->make_observable()
          .from_container(
            std::vector{ctx->make_observable().just(1).as_observable(),
                        ctx->make_observable().just(2).as_observable(),
                        make_insta_empty<int>()})
          .concat(ctx->make_observable().just(3))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->completed());
        CHECK_EQ(snk->buf, std::vector<int>({1, 2, 3}));
      }
    }
    WHEN("concatenating it but disposing the operator") {
      THEN("the observer only an on_complete event") {
        auto snk = flow::make_passive_observer<int>();
        auto sub = ctx->make_observable()
                     .never<flow::observable<int>>()
                     .concat()
                     .subscribe(snk->as_observer());
        ctx->run();
        sub.dispose();
        ctx->run();
        CHECK(snk->completed());
        CHECK(snk->buf.empty());
      }
    }
  }
  GIVEN("two observables, whereas the first produces an error") {
    WHEN("concatenating them to a single publisher") {
      THEN("the observer only receives an error") {
        auto outputs = std::vector<int>{};
        auto r1 = ctx->make_observable().fail<int>(sec::runtime_error);
        auto r2 = ctx->make_observable().iota(1).take(3);
        auto snk = flow::make_auto_observer<int>();
        ctx->make_observable()
          .concat(std::move(r1), std::move(r2))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->aborted());
        CHECK(snk->buf.empty());
        CHECK_EQ(snk->err, sec::runtime_error);
      }
    }
  }
  GIVEN("two observables, whereas the second one produces an error") {
    WHEN("concatenating them to a single publisher") {
      THEN("the observer receives the first set of items and then an error") {
        auto outputs = std::vector<int>{};
        auto r1 = ctx->make_observable().iota(1).take(3);
        auto r2 = ctx->make_observable().fail<int>(sec::runtime_error);
        auto snk = flow::make_auto_observer<int>();
        ctx->make_observable()
          .concat(std::move(r1), std::move(r2))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->aborted());
        CHECK_EQ(snk->buf, std::vector<int>({1, 2, 3}));
        CHECK_EQ(snk->err, sec::runtime_error);
      }
    }
  }
}

SCENARIO("empty concat operators only call on_complete") {
  GIVEN("a concat operator with no inputs") {
    WHEN("subscribing to it") {
      THEN("the observer only receives an on_complete event") {
        auto snk = flow::make_auto_observer<int>();
        auto sub = make_operator<int>()->subscribe(snk->as_observer());
        ctx->run();
        CHECK(sub.disposed());
        CHECK(snk->completed());
        CHECK(snk->buf.empty());
      }
    }
  }
}

SCENARIO("the concat operator disposes unexpected subscriptions") {
  GIVEN("a concat operator with no inputs") {
    WHEN("subscribing to it") {
      THEN("the observer only receives an on_complete event") {
        auto snk = flow::make_passive_observer<int>();
        auto r1 = ctx->make_observable().just(1).as_observable();
        auto r2 = ctx->make_observable().just(2).as_observable();
        auto uut = raw_sub(snk->as_observer(), r1, r2);
        auto sub = make_counted<flow::passive_subscription_impl>();
        ctx->run();
        CHECK(!sub->disposed());
        uut->fwd_on_subscribe(42, flow::subscription{sub});
        CHECK(sub->disposed());
        snk->request(127);
        ctx->run();
        CHECK(snk->completed());
        CHECK_EQ(snk->buf, std::vector<int>({1, 2}));
      }
    }
  }
}

END_FIXTURE_SCOPE()
