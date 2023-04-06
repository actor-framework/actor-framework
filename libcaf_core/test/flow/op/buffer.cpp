// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.op.buffer

#include "caf/flow/op/buffer.hpp"

#include "core-test.hpp"

#include "caf/flow/coordinator.hpp"
#include "caf/flow/item_publisher.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;
using namespace std::literals;

namespace {

constexpr auto fwd_data = flow::op::buffer_input_t{};

constexpr auto fwd_ctrl = flow::op::buffer_emit_t{};

struct skip_trait {
  static constexpr bool skip_empty = true;
  using input_type = int;
  using output_type = cow_vector<int>;
  using select_token_type = int64_t;

  output_type operator()(const std::vector<input_type>& xs) {
    return output_type{xs};
  }
};

struct noskip_trait {
  static constexpr bool skip_empty = false;
  using input_type = int;
  using output_type = cow_vector<int>;
  using select_token_type = int64_t;

  output_type operator()(const std::vector<input_type>& xs) {
    return output_type{xs};
  }
};

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  ~fixture() {
    ctx->run();
  }

  // Similar to buffer::subscribe, but returns a buffer_sub pointer instead of
  // type-erasing it into a disposable.
  template <class Trait = noskip_trait>
  auto raw_sub(size_t max_items, flow::observable<int> in,
               flow::observable<int64_t> select,
               flow::observer<cow_vector<int>> out) {
    using sub_t = flow::op::buffer_sub<Trait>;
    auto ptr = make_counted<sub_t>(ctx.get(), max_items, out);
    ptr->init(in, select);
    out.on_subscribe(flow::subscription{ptr});
    return ptr;
  }

  template <class Impl>
  void add_subs(intrusive_ptr<Impl> uut) {
    auto data_sub = make_counted<flow::passive_subscription_impl>();
    uut->fwd_on_subscribe(fwd_data, flow::subscription{std::move(data_sub)});
    auto ctrl_sub = make_counted<flow::passive_subscription_impl>();
    uut->fwd_on_subscribe(fwd_ctrl, flow::subscription{std::move(ctrl_sub)});
  }

  template <class T>
  auto trivial_obs() {
    return flow::make_trivial_observable<T>(ctx.get());
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the buffer operator groups items together") {
  GIVEN("an observable") {
    WHEN("calling .buffer(3)") {
      THEN("the observer receives values in groups of three") {
        auto inputs = std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128};
        auto outputs = std::vector<cow_vector<int>>{};
        auto expected = std::vector<cow_vector<int>>{
          cow_vector<int>{1, 2, 4},
          cow_vector<int>{8, 16, 32},
          cow_vector<int>{64, 128},
        };
        ctx->make_observable()
          .from_container(inputs) //
          .buffer(3)
          .for_each([&outputs](const cow_vector<int>& xs) {
            outputs.emplace_back(xs);
          });
        ctx->run();
        CHECK_EQ(outputs, expected);
      }
    }
  }
}

SCENARIO("the buffer operator forces items at regular intervals") {
  GIVEN("an observable") {
    WHEN("calling .buffer(3, 1s)") {
      THEN("the observer receives values in groups of three or after 1s") {
        auto outputs = std::make_shared<std::vector<cow_vector<int>>>();
        auto expected = std::vector<cow_vector<int>>{
          cow_vector<int>{1, 2, 4}, cow_vector<int>{8, 16, 32},
          cow_vector<int>{},        cow_vector<int>{64},
          cow_vector<int>{},        cow_vector<int>{128, 256, 512},
        };
        auto pub = flow::item_publisher<int>{ctx.get()};
        sys.spawn([&pub, outputs](caf::event_based_actor* self) {
          pub.as_observable()
            .observe_on(self) //
            .buffer(3, 1s)
            .for_each([outputs](const cow_vector<int>& xs) {
              outputs->emplace_back(xs);
            });
        });
        sched.run();
        MESSAGE("emit the first six items");
        pub.push({1, 2, 4, 8, 16, 32});
        ctx->run_some();
        sched.run();
        MESSAGE("force an empty buffer");
        advance_time(1s);
        sched.run();
        MESSAGE("force a buffer with a single element");
        pub.push(64);
        ctx->run_some();
        sched.run();
        advance_time(1s);
        sched.run();
        MESSAGE("force an empty buffer");
        advance_time(1s);
        sched.run();
        MESSAGE("emit the last items and close the source");
        pub.push({128, 256, 512});
        pub.close();
        ctx->run_some();
        sched.run();
        advance_time(1s);
        sched.run();
        CHECK_EQ(*outputs, expected);
      }
    }
  }
}

SCENARIO("the buffer operator forwards errors") {
  GIVEN("an observable that produces some values followed by an error") {
    WHEN("calling .buffer() on it") {
      THEN("the observer receives the values and then the error") {
        auto outputs = std::make_shared<std::vector<cow_vector<int>>>();
        auto err = std::make_shared<error>();
        sys.spawn([outputs, err](caf::event_based_actor* self) {
          auto obs = self->make_observable();
          obs.iota(1)
            .take(17)
            .concat(obs.fail<int>(make_error(caf::sec::runtime_error)))
            .buffer(7, 1s)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([outputs](const cow_vector<int>& xs) {
              outputs->emplace_back(xs);
            });
        });
        sched.run();
        auto expected = std::vector<cow_vector<int>>{
          cow_vector<int>{1, 2, 3, 4, 5, 6, 7},
          cow_vector<int>{8, 9, 10, 11, 12, 13, 14},
          cow_vector<int>{15, 16, 17},
        };
        CHECK_EQ(*outputs, expected);
        CHECK_EQ(*err, caf::sec::runtime_error);
      }
    }
  }
  GIVEN("an observable that produces only an error") {
    WHEN("calling .buffer() on it") {
      THEN("the observer receives the error") {
        auto outputs = std::make_shared<std::vector<cow_vector<int>>>();
        auto err = std::make_shared<error>();
        sys.spawn([outputs, err](caf::event_based_actor* self) {
          self->make_observable()
            .fail<int>(make_error(caf::sec::runtime_error))
            .buffer(3, 1s)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([outputs](const cow_vector<int>& xs) {
              outputs->emplace_back(xs);
            });
        });
        sched.run();
        CHECK(outputs->empty());
        CHECK_EQ(*err, caf::sec::runtime_error);
      }
    }
  }
}

SCENARIO("buffers start to emit items once subscribed") {
  GIVEN("a buffer operator") {
    WHEN("the selector never calls on_subscribe") {
      THEN("the buffer still emits batches") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto grd = make_unsubscribe_guard(snk);
        auto uut = raw_sub(3, flow::make_nil_observable<int>(ctx.get()),
                           flow::make_nil_observable<int64_t>(ctx.get()),
                           snk->as_observer());
        auto data_sub = make_counted<flow::passive_subscription_impl>();
        uut->fwd_on_subscribe(fwd_data, flow::subscription{data_sub});
        ctx->run();
        REQUIRE_GE(data_sub->demand, 3u);
        for (int i = 0; i < 3; ++i)
          uut->fwd_on_next(fwd_data, i);
        ctx->run();
        CHECK_EQ(snk->buf.size(), 0u);
        snk->request(17);
        ctx->run();
        if (CHECK_EQ(snk->buf.size(), 1u))
          CHECK_EQ(snk->buf[0], cow_vector<int>({0, 1, 2}));
      }
    }
  }
}

SCENARIO("buffers never subscribe to their control observable on error") {
  GIVEN("a buffer operator") {
    WHEN("the data observable calls on_error on subscribing it") {
      THEN("the buffer never tries to subscribe to their control observable") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto cnt = std::make_shared<size_t>(0);
        auto uut = raw_sub(3,
                           ctx->make_observable().fail<int>(sec::runtime_error),
                           flow::make_nil_observable<int64_t>(ctx.get(), cnt),
                           snk->as_observer());
        ctx->run();
        CHECK(snk->aborted());
        CHECK_EQ(*cnt, 0u);
      }
    }
  }
}

SCENARIO("buffers dispose unexpected subscriptions") {
  GIVEN("an initialized buffer operator") {
    WHEN("calling on_subscribe with unexpected subscriptions") {
      THEN("the buffer disposes them immediately") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto grd = make_unsubscribe_guard(snk);
        auto uut = raw_sub(3, flow::make_nil_observable<int>(ctx.get()),
                           flow::make_nil_observable<int64_t>(ctx.get()),
                           snk->as_observer());
        auto data_sub = make_counted<flow::passive_subscription_impl>();
        auto ctrl_sub = make_counted<flow::passive_subscription_impl>();
        uut->fwd_on_subscribe(fwd_data, flow::subscription{data_sub});
        uut->fwd_on_subscribe(fwd_ctrl, flow::subscription{ctrl_sub});
        ctx->run();
        auto data_sub_2 = make_counted<flow::passive_subscription_impl>();
        auto ctrl_sub_2 = make_counted<flow::passive_subscription_impl>();
        uut->fwd_on_subscribe(fwd_data, flow::subscription{data_sub_2});
        uut->fwd_on_subscribe(fwd_ctrl, flow::subscription{ctrl_sub_2});
        ctx->run();
        CHECK(!uut->disposed());
        CHECK(!data_sub->disposed());
        CHECK(!ctrl_sub->disposed());
        CHECK(data_sub_2->disposed());
        CHECK(ctrl_sub_2->disposed());
      }
    }
  }
}

SCENARIO("buffers emit final items after an on_error event") {
  GIVEN("an initialized buffer operator") {
    WHEN("calling on_error(data) on a buffer without pending data") {
      THEN("the buffer forward on_error immediately") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        snk->request(42);
        ctx->run();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        CHECK_EQ(uut->pending(), 0u);
        uut->fwd_on_error(fwd_data, sec::runtime_error);
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>({1, 2, 3})});
        CHECK(snk->aborted());
      }
    }
    WHEN("calling on_error(data) on a buffer with pending data") {
      THEN("the buffer still emits pending data before closing") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        ctx->run();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        CHECK_EQ(uut->pending(), 2u);
        uut->fwd_on_error(fwd_data, sec::runtime_error);
        CHECK(snk->buf.empty());
        CHECK(!snk->aborted());
        snk->request(42);
        ctx->run();
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>({1, 2})});
        CHECK(snk->aborted());
      }
    }
    WHEN("calling on_error(control) on a buffer without pending data") {
      THEN("the buffer forward on_error immediately") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        snk->request(42);
        ctx->run();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        CHECK_EQ(uut->pending(), 0u);
        uut->fwd_on_error(fwd_ctrl, sec::runtime_error);
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>({1, 2, 3})});
        CHECK(snk->aborted());
      }
    }
    WHEN("calling on_error(control) on a buffer with pending data") {
      THEN("the buffer still emits pending data before closing") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        ctx->run();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        CHECK_EQ(uut->pending(), 2u);
        uut->fwd_on_error(fwd_ctrl, sec::runtime_error);
        CHECK(snk->buf.empty());
        CHECK(!snk->aborted());
        snk->request(42);
        ctx->run();
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>({1, 2})});
        CHECK(snk->aborted());
      }
    }
  }
}

SCENARIO("buffers emit final items after an on_complete event") {
  GIVEN("an initialized buffer operator") {
    WHEN("calling on_complete(data) on a buffer without pending data") {
      THEN("the buffer forward on_complete immediately") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        snk->request(42);
        ctx->run();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        CHECK_EQ(uut->pending(), 0u);
        uut->fwd_on_complete(fwd_data);
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>({1, 2, 3})});
        CHECK(snk->completed());
      }
    }
    WHEN("calling on_complete(data) on a buffer with pending data") {
      THEN("the buffer still emits pending data before closing") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        ctx->run();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        CHECK_EQ(uut->pending(), 2u);
        uut->fwd_on_complete(fwd_data);
        CHECK(snk->buf.empty());
        CHECK(!snk->completed());
        snk->request(42);
        ctx->run();
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>({1, 2})});
        CHECK(snk->completed());
      }
    }
    WHEN("calling on_complete(control) on a buffer without pending data") {
      THEN("the buffer raises an error immediately") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        snk->request(42);
        ctx->run();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        CHECK_EQ(uut->pending(), 0u);
        uut->fwd_on_complete(fwd_ctrl);
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>({1, 2, 3})});
        CHECK(snk->aborted());
      }
    }
    WHEN("calling on_complete(control) on a buffer with pending data") {
      THEN("the buffer raises an error after shipping pending items") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        ctx->run();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        CHECK_EQ(uut->pending(), 2u);
        uut->fwd_on_complete(fwd_ctrl);
        CHECK(snk->buf.empty());
        CHECK(!snk->completed());
        snk->request(42);
        ctx->run();
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>({1, 2})});
        CHECK(snk->aborted());
      }
    }
  }
}

SCENARIO("skip policies suppress empty batches") {
  GIVEN("a buffer operator") {
    WHEN("the control observable fires with no pending data") {
      THEN("the operator omits the batch") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto grd = make_unsubscribe_guard(snk);
        auto uut = raw_sub<skip_trait>(3, trivial_obs<int>(),
                                       trivial_obs<int64_t>(),
                                       snk->as_observer());
        add_subs(uut);
        snk->request(42);
        ctx->run();
        uut->fwd_on_next(fwd_ctrl, 1);
        ctx->run();
        CHECK(snk->buf.empty());
      }
    }
    WHEN("the control observable fires with pending data") {
      THEN("the operator emits a partial batch") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto grd = make_unsubscribe_guard(snk);
        auto uut = raw_sub<skip_trait>(3, trivial_obs<int>(),
                                       trivial_obs<int64_t>(),
                                       snk->as_observer());
        add_subs(uut);
        snk->request(42);
        ctx->run();
        uut->fwd_on_next(fwd_data, 17);
        uut->fwd_on_next(fwd_ctrl, 1);
        ctx->run();
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>{17}});
      }
    }
  }
}

SCENARIO("no-skip policies emit empty batches") {
  GIVEN("a buffer operator") {
    WHEN("the control observable fires with no pending data") {
      THEN("the operator emits an empty batch") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto grd = make_unsubscribe_guard(snk);
        auto uut = raw_sub<noskip_trait>(3, trivial_obs<int>(),
                                         trivial_obs<int64_t>(),
                                         snk->as_observer());
        add_subs(uut);
        snk->request(42);
        ctx->run();
        uut->fwd_on_next(fwd_ctrl, 1);
        ctx->run();
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>()});
      }
    }
    WHEN("the control observable fires with pending data") {
      THEN("the operator emits a partial batch") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto grd = make_unsubscribe_guard(snk);
        auto uut = raw_sub<noskip_trait>(3, trivial_obs<int>(),
                                         trivial_obs<int64_t>(),
                                         snk->as_observer());
        add_subs(uut);
        snk->request(42);
        ctx->run();
        uut->fwd_on_next(fwd_data, 17);
        uut->fwd_on_next(fwd_ctrl, 1);
        ctx->run();
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>{17}});
      }
    }
  }
}

SCENARIO("disposing a buffer operator completes the flow") {
  GIVEN("a buffer operator") {
    WHEN("disposing the subscription operator of the operator") {
      THEN("the observer receives an on_complete event") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub<skip_trait>(3, trivial_obs<int>(),
                                       trivial_obs<int64_t>(),
                                       snk->as_observer());
        add_subs(uut);
        snk->request(42);
        ctx->run();
        uut->dispose();
        ctx->run();
        CHECK(snk->completed());
      }
    }
  }
}

SCENARIO("on_request actions can turn into no-ops") {
  GIVEN("a buffer operator") {
    WHEN("the sink requests more data right before a timeout triggers") {
      THEN("the batch gets shipped and the on_request action does nothing") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto grd = make_unsubscribe_guard(snk);
        auto uut = raw_sub<skip_trait>(3, trivial_obs<int>(),
                                       trivial_obs<int64_t>(),
                                       snk->as_observer());
        add_subs(uut);
        ctx->run();
        // Add three items that we can't push yet because no downstream demand.
        for (int i = 0; i < 3; ++i)
          uut->fwd_on_next(fwd_data, i);
        CHECK(uut->can_emit());
        CHECK_EQ(uut->pending(), 3u);
        // Add demand, which triggers an action - but don't run it yet.
        snk->request(42);
        CHECK_EQ(uut->pending(), 3u);
        // Fire on_next on the control channel to force the batch out.
        uut->fwd_on_next(fwd_ctrl, 1);
        CHECK_EQ(uut->pending(), 0u);
        // Run the scheduled action: turns into a no-op now.
        ctx->run();
        CHECK_EQ(snk->buf, std::vector{cow_vector<int>({0, 1, 2})});
      }
    }
  }
}

END_FIXTURE_SCOPE()
