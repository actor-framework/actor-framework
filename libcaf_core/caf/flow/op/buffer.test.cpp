// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/op/buffer.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/all.hpp"

using namespace caf;
using namespace std::literals;

namespace {

constexpr auto fwd_data = caf::flow::op::buffer_input_t{};

constexpr auto fwd_ctrl = caf::flow::op::buffer_emit_t{};

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

struct fixture_deterministic : test::fixture::deterministic {
  caf::flow::scoped_coordinator_ptr ctx = caf::flow::make_scoped_coordinator();

  ~fixture_deterministic() {
    ctx->run();
  }

  template <class Impl>
  void add_subs(intrusive_ptr<Impl> uut) {
    auto data_sub
      = make_counted<test::fixture::flow::passive_subscription_impl>(ctx.get());
    uut->fwd_on_subscribe(fwd_data, flow::subscription{std::move(data_sub)});
    auto ctrl_sub
      = make_counted<test::fixture::flow::passive_subscription_impl>(ctx.get());
    uut->fwd_on_subscribe(fwd_ctrl, flow::subscription{std::move(ctrl_sub)});
  }

  template <class T>
  auto trivial_obs() {
    return test::fixture::flow::make_trivial_observable<T>(ctx.get());
  }
};

struct fixture_flow : test::fixture::flow {
  // Similar to buffer::subscribe, but returns a buffer_sub pointer instead of
  //  type-erasing it into a disposable.
  template <class Trait = noskip_trait>
  auto raw_sub(size_t max_items, caf::flow::observable<int> in,
               caf::flow::observable<int64_t> select,
               caf::flow::observer<cow_vector<int>> out) {
    using sub_t = caf::flow::op::buffer_sub<Trait>;
    auto ptr = make_counted<sub_t>(coordinator(), max_items, out);
    ptr->init(in, select);
    out.on_subscribe(caf::flow::subscription{ptr});
    return ptr;
  }

  template <class Impl>
  void add_subs(intrusive_ptr<Impl> uut) {
    auto data_sub
      = make_counted<flow::passive_subscription_impl>(coordinator());
    uut->fwd_on_subscribe(fwd_data,
                          caf::flow::subscription{std::move(data_sub)});
    auto ctrl_sub = make_counted<passive_subscription_impl>(coordinator());
    uut->fwd_on_subscribe(fwd_ctrl,
                          caf::flow::subscription{std::move(ctrl_sub)});
  }

  template <class T>
  auto trivial_obs() {
    return flow::make_trivial_observable<T>(coordinator());
  }
};

WITH_FIXTURE(fixture_deterministic) {

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
        check_eq(outputs, expected);
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
        auto closed = std::make_shared<bool>(false);
        auto pub = caf::flow::multicaster<int>{ctx.get()};
        sys.spawn([&pub, outputs, closed](caf::event_based_actor* self) {
          pub.as_observable()
            .observe_on(self) //
            .buffer(3, 1s)
            .do_on_complete([closed] { *closed = true; })
            .for_each([outputs](const cow_vector<int>& xs) {
              outputs->emplace_back(xs);
            });
        });
        dispatch_messages();
        print_info("emit the first six items");
        pub.push({1, 2, 4, 8, 16, 32});
        ctx->run_some();
        dispatch_messages();
        print_info("force an empty buffer");
        advance_time(1s);
        dispatch_messages();
        print_info("force a buffer with a single element");
        pub.push(64);
        ctx->run_some();
        dispatch_messages();
        advance_time(1s);
        dispatch_messages();
        print_info("force an empty buffer");
        advance_time(1s);
        dispatch_messages();
        print_info("emit the last items and close the source");
        pub.push({128, 256, 512});
        pub.close();
        ctx->run_some();
        dispatch_messages();
        advance_time(1s);
        dispatch_messages();
        check_eq(*outputs, expected);
        check(*closed);
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
        dispatch_messages();
        auto expected = std::vector<cow_vector<int>>{
          cow_vector<int>{1, 2, 3, 4, 5, 6, 7},
          cow_vector<int>{8, 9, 10, 11, 12, 13, 14},
          cow_vector<int>{15, 16, 17},
        };
        check_eq(*outputs, expected);
        check_eq(*err, caf::sec::runtime_error);
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
        dispatch_messages();
        check(outputs->empty());
        check_eq(*err, caf::sec::runtime_error);
      }
    }
  }
}

} // WITH_FIXTURE(fixture_deterministic)

WITH_FIXTURE(fixture_flow) {

SCENARIO("buffers start to emit items once subscribed") {
  GIVEN("a buffer operator") {
    WHEN("the selector never calls on_subscribe") {
      THEN("the buffer still emits batches") {
        auto snk = make_passive_observer<cow_vector<int>>();
        auto grd = make_unsubscribe_guard(snk);
        auto uut = raw_sub(3, make_nil_observable<int>(coordinator()),
                           make_nil_observable<int64_t>(coordinator()),
                           snk->as_observer());
        auto data_sub
          = make_counted<flow::passive_subscription_impl>(coordinator());
        uut->fwd_on_subscribe(fwd_data, caf::flow::subscription{data_sub});
        run_flows();
        require_ge(data_sub->demand, 3u);
        for (int i = 0; i < 3; ++i)
          uut->fwd_on_next(fwd_data, i);
        run_flows();
        check_eq(snk->buf.size(), 0u);
        snk->request(17);
        run_flows();
        if (check_eq(snk->buf.size(), 1u))
          check_eq(snk->buf[0], cow_vector<int>({0, 1, 2}));
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
        auto uut = raw_sub(
          3, coordinator()->make_observable().fail<int>(sec::runtime_error),
          make_nil_observable<int64_t>(coordinator(), cnt), snk->as_observer());
        run_flows();
        check(snk->aborted());
        check_eq(*cnt, 0u);
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
        auto uut = raw_sub(3, flow::make_nil_observable<int>(coordinator()),
                           flow::make_nil_observable<int64_t>(coordinator()),
                           snk->as_observer());
        auto data_sub
          = make_counted<flow::passive_subscription_impl>(coordinator());
        auto ctrl_sub
          = make_counted<flow::passive_subscription_impl>(coordinator());
        uut->fwd_on_subscribe(fwd_data, caf::flow::subscription{data_sub});
        uut->fwd_on_subscribe(fwd_ctrl, caf::flow::subscription{ctrl_sub});
        run_flows();
        auto data_sub_2
          = make_counted<flow::passive_subscription_impl>(coordinator());
        auto ctrl_sub_2
          = make_counted<flow::passive_subscription_impl>(coordinator());
        uut->fwd_on_subscribe(fwd_data, caf::flow::subscription{data_sub_2});
        uut->fwd_on_subscribe(fwd_ctrl, caf::flow::subscription{ctrl_sub_2});
        run_flows();
        check(!uut->disposed());
        check(!data_sub->disposed());
        check(!ctrl_sub->disposed());
        check(data_sub_2->disposed());
        check(ctrl_sub_2->disposed());
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
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        check_eq(uut->pending(), 0u);
        uut->fwd_on_error(fwd_data, sec::runtime_error);
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2, 3})});
        check(snk->aborted());
      }
    }
    WHEN("calling on_error(data) on a buffer with pending data") {
      THEN("the buffer still emits pending data before closing") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), 2u);
        uut->fwd_on_error(fwd_data, sec::runtime_error);
        check(snk->buf.empty());
        check(!snk->aborted());
        snk->request(42);
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2})});
        check(snk->aborted());
      }
    }
    WHEN("calling on_error(control) on a buffer without pending data") {
      THEN("the buffer forward on_error immediately") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        snk->request(42);
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        check_eq(uut->pending(), 0u);
        uut->fwd_on_error(fwd_ctrl, sec::runtime_error);
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2, 3})});
        check(snk->aborted());
      }
    }
    WHEN("calling on_error(control) on a buffer with pending data") {
      THEN("the buffer still emits pending data before closing") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), 2u);
        uut->fwd_on_error(fwd_ctrl, sec::runtime_error);
        check(snk->buf.empty());
        check(!snk->aborted());
        snk->request(42);
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2})});
        check(snk->aborted());
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
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        check_eq(uut->pending(), 0u);
        uut->fwd_on_complete(fwd_data);
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2, 3})});
        check(snk->completed());
      }
    }
    WHEN("calling on_complete(data) on a buffer with pending data") {
      THEN("the buffer still emits pending data before closing") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), 2u);
        uut->fwd_on_complete(fwd_data);
        check(snk->buf.empty());
        check(!snk->completed());
        snk->request(42);
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2})});
        check(snk->completed());
      }
    }
    WHEN("calling on_complete(control) on a buffer without pending data") {
      THEN("the buffer raises an error immediately") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        snk->request(42);
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        check_eq(uut->pending(), 0u);
        uut->fwd_on_complete(fwd_ctrl);
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2, 3})});
        check(snk->aborted());
      }
    }
    WHEN("calling on_complete(control) on a buffer with pending data") {
      THEN("the buffer raises an error after shipping pending items") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, trivial_obs<int>(), trivial_obs<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), 2u);
        uut->fwd_on_complete(fwd_ctrl);
        check(snk->buf.empty());
        check(!snk->completed());
        snk->request(42);
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2})});
        check(snk->aborted());
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
        run_flows();
        uut->fwd_on_next(fwd_ctrl, 1);
        run_flows();
        check(snk->buf.empty());
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
        run_flows();
        uut->fwd_on_next(fwd_data, 17);
        uut->fwd_on_next(fwd_ctrl, 1);
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>{17}});
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
        run_flows();
        uut->fwd_on_next(fwd_ctrl, 1);
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>()});
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
        run_flows();
        uut->fwd_on_next(fwd_data, 17);
        uut->fwd_on_next(fwd_ctrl, 1);
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>{17}});
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
        run_flows();
        uut->dispose();
        run_flows();
        check(snk->completed());
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
        run_flows();
        // Add three items that we can't push yet because no downstream demand.
        for (int i = 0; i < 3; ++i)
          uut->fwd_on_next(fwd_data, i);
        check(uut->can_emit());
        check_eq(uut->pending(), 3u);
        // Add demand, which triggers an action - but don't run it yet.
        snk->request(42);
        check_eq(uut->pending(), 3u);
        // Fire on_next on the control channel to force the batch out.
        uut->fwd_on_next(fwd_ctrl, 1);
        check_eq(uut->pending(), 0u);
        // Run the scheduled action: turns into a no-op now.
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>({0, 1, 2})});
      }
    }
  }
}

} // WITH_FIXTURE(fixture_flow)

} // namespace
