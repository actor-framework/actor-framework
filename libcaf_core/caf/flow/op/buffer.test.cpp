// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/buffer.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/outline.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/flow/multicaster.hpp"
#include "caf/log/test.hpp"

#include <string>

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

struct fixture : test::fixture::deterministic, test::fixture::flow {
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

  template <class T>
  auto make_never_sub(caf::flow::observer<T> out) {
    return make_counted<caf::flow::op::never_sub<T>>(coordinator(), out);
  }
};

} // namespace

WITH_FIXTURE(fixture) {

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
        check_eq(collect(make_observable().from_container(inputs).buffer(3)),
                 expected);
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
        auto pub = caf::flow::multicaster<int>{coordinator()};
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
        log::test::debug("emit the first six items");
        pub.push({1, 2, 4, 8, 16, 32});
        run_flows();
        dispatch_messages();
        log::test::debug("force an empty buffer");
        advance_time(1s);
        dispatch_messages();
        log::test::debug("force a buffer with a single element");
        pub.push(64);
        run_flows();
        dispatch_messages();
        advance_time(1s);
        dispatch_messages();
        log::test::debug("force an empty buffer");
        advance_time(1s);
        dispatch_messages();
        log::test::debug("emit the last items and close the source");
        pub.push({128, 256, 512});
        pub.close();
        run_flows();
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

SCENARIO("buffers dispose unexpected subscriptions") {
  GIVEN("an initialized buffer operator") {
    WHEN("calling on_subscribe with unexpected subscriptions") {
      THEN("the buffer disposes them immediately") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        auto data_sub = make_never_sub<cow_vector<int>>(snk->as_observer());
        auto ctrl_sub = make_never_sub<cow_vector<int>>(snk->as_observer());
        uut->fwd_on_subscribe(fwd_data, caf::flow::subscription{data_sub});
        uut->fwd_on_subscribe(fwd_ctrl, caf::flow::subscription{ctrl_sub});
        check(snk->subscribed());
        check(!uut->disposed());
        run_flows();
        check(data_sub->disposed());
        check(ctrl_sub->disposed());
        uut->dispose();
        run_flows();
        check(uut->disposed());
      }
    }
  }
}

SCENARIO("buffers emit final items after an on_error event") {
  GIVEN("an initialized buffer operator") {
    WHEN("calling on_error(data) on a buffer without pending data") {
      THEN("the buffer forward on_error immediately") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, make_observable().never<int>(),
                           make_observable().never<int64_t>(),
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
        auto uut = raw_sub(3, make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), 2u);
        uut->fwd_on_error(fwd_data, sec::runtime_error);
        check(snk->buf.empty());
        check(!snk->aborted());
        snk->request(42);
        uut->dispose();
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2})});
        check(snk->aborted());
      }
    }
    WHEN("calling on_error(control) on a buffer without pending data") {
      THEN("the buffer forward on_error immediately") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, make_observable().never<int>(),
                           make_observable().never<int64_t>(),
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
        uut->dispose();
        run_flows();
      }
    }
    WHEN("calling on_error(control) on a buffer with pending data") {
      THEN("the buffer still emits pending data before closing") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), 2u);
        uut->fwd_on_error(fwd_ctrl, sec::runtime_error);
        check(snk->buf.empty());
        check(!snk->aborted());
        snk->request(42);
        uut->dispose();
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
        auto uut = raw_sub(3, make_observable().never<int>(),
                           make_observable().never<int64_t>(),
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
        auto uut = raw_sub(3, make_observable().never<int>(),
                           make_observable().never<int64_t>(),
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
        auto uut = raw_sub(3, make_observable().never<int>(),
                           make_observable().never<int64_t>(),
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
        auto uut = raw_sub(3, make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), 2u);
        uut->fwd_on_complete(fwd_ctrl);
        check(snk->buf.empty());
        check(!snk->completed());
        snk->request(42);
        uut->dispose();
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2})});
        check(snk->aborted());
      }
    }
  }
}

OUTLINE("buffers deliver pending items before terminal events") {
  GIVEN("a buffer in the <mode> case") {
    auto mode = block_parameters<std::string>();
    auto snk = make_passive_observer<cow_vector<int>>();
    auto uut = raw_sub(3, make_observable().never<int>(),
                       make_observable().never<int64_t>(), snk->as_observer());
    auto expected = std::vector<cow_vector<int>>{};
    auto pending = cow_vector<int>{};
    if (mode == "partial") {
      // An empty flush spends demand while the requested items are in flight.
      snk->request(1);
      uut->fwd_on_next(fwd_ctrl, 1);
      uut->fwd_on_next(fwd_data, 1);
      uut->fwd_on_next(fwd_data, 2);
      expected.emplace_back();
      pending = cow_vector<int>{1, 2};
    } else if (mode == "forced_partial" || mode == "remaining_demand") {
      snk->request(mode == "remaining_demand" ? 2 : 1);
      uut->fwd_on_next(fwd_data, 1);
      uut->fwd_on_next(fwd_ctrl, 1);
      uut->fwd_on_next(fwd_data, 2);
      if (mode == "forced_partial")
        uut->fwd_on_next(fwd_ctrl, 2);
      expected.emplace_back(cow_vector<int>{1});
      pending = cow_vector<int>{2};
    } else {
      require_eq(mode, "forced_empty");
      uut->fwd_on_next(fwd_ctrl, 1);
    }
    check_eq(uut->pending(), pending.size());
    check_eq(snk->buf, expected);
    WHEN("the <source> source sends <event>") {
      auto [source, event] = block_parameters<std::string, std::string>();
      auto expected_error = sec::none;
      if (event == "error")
        expected_error = sec::runtime_error;
      else if (source == "control")
        expected_error = sec::end_of_stream;
      if (source == "data") {
        if (event == "complete")
          uut->fwd_on_complete(fwd_data);
        else
          uut->fwd_on_error(fwd_data, sec::runtime_error);
      } else {
        if (event == "complete")
          uut->fwd_on_complete(fwd_ctrl);
        else
          uut->fwd_on_error(fwd_ctrl, sec::runtime_error);
      }
      THEN("buffered items precede the terminal event and late tokens are "
           "ignored") {
        check(!uut->running());
        uut->fwd_on_next(fwd_ctrl, 99);
        if (!pending.empty() && mode != "remaining_demand") {
          check(uut->can_emit());
          check(snk->subscribed());
          check_eq(snk->buf, expected);
          snk->request(1);
          run_flows();
        }
        if (!pending.empty())
          expected.emplace_back(pending);
        check_eq(snk->buf, expected);
        if (expected_error == sec::none) {
          check(snk->completed());
        } else {
          check(snk->aborted());
          check_eq(snk->err, expected_error);
        }
        check(uut->disposed());
        check(!uut->can_emit());
      }
    }
  }
  EXAMPLES = R"(
    | mode             | source  | event    |
    | partial          | data    | complete |
    | partial          | data    | error    |
    | partial          | control | complete |
    | partial          | control | error    |
    | forced_partial   | data    | complete |
    | forced_partial   | data    | error    |
    | forced_partial   | control | complete |
    | forced_partial   | control | error    |
    | forced_empty     | data    | complete |
    | forced_empty     | data    | error    |
    | forced_empty     | control | complete |
    | forced_empty     | control | error    |
    | remaining_demand | data    | complete |
    | remaining_demand | data    | error    |
    | remaining_demand | control | complete |
    | remaining_demand | control | error    |
  )";
}

SCENARIO("buffers pull items from the source only against demand") {
  GIVEN("a buffer operator on a source that always has items available") {
    WHEN("the observer signals no demand") {
      THEN("the operator buffers nothing and terminates without demand") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, make_observable().iota(1).as_observable(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        run_flows();
        check_eq(uut->pending(), 0u);
        uut->fwd_on_complete(fwd_data);
        check(snk->completed());
        check(snk->buf.empty());
      }
    }
    WHEN("the observer requests a single batch") {
      THEN("the operator pulls one batch and then stops reading ahead") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub(3, make_observable().iota(1).as_observable(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        snk->request(1);
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2, 3})});
        check_eq(uut->pending(), 0u);
        uut->fwd_on_complete(fwd_data);
        check(snk->completed());
      }
    }
  }
  GIVEN("a finite source with fewer items than the batch size") {
    WHEN("the observer requests one batch") {
      THEN("the partial batch and completion arrive without another request") {
        auto snk = make_passive_observer<cow_vector<int>>();
        make_observable()
          .from_container(std::vector{1, 2})
          .buffer(3)
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->buf.empty());
        check(snk->subscribed());
        snk->request(1);
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>{1, 2}});
        check(snk->completed());
      }
    }
  }
}

SCENARIO("the interval overload spends demand on timer-forced batches") {
  GIVEN("a .buffer(3, 1s) whose observer requests a single batch") {
    WHEN("the period elapses again after already spending that demand") {
      THEN("the buffer preserves the forced flush and ships it alone as soon "
           "as demand returns, without pulling further values first") {
        auto pub = caf::flow::multicaster<int>{coordinator()};
        auto snk_hdl = std::make_shared<
          intrusive_ptr<passive_observer<cow_vector<int>>>>();
        auto sub_hdl = std::make_shared<disposable>();
        sys.spawn([&pub, snk_hdl, sub_hdl](caf::event_based_actor* self) {
          auto snk = self->add_child(
            std::in_place_type<passive_observer<cow_vector<int>>>);
          *snk_hdl = snk;
          *sub_hdl = pub.as_observable()
                       .observe_on(self) //
                       .buffer(3, 1s)
                       .subscribe(snk->as_observer());
        });
        dispatch_messages();
        auto snk = *snk_hdl;
        require(snk != nullptr);
        snk->parent()->schedule_fn([snk] { snk->request(1); });
        dispatch_messages();
        pub.push({1, 2});
        run_flows();
        dispatch_messages();
        // Not a full batch yet, so nothing goes out before the period elapses.
        check(snk->buf.empty());
        advance_time(1s);
        dispatch_messages();
        check_eq(snk->buf, std::vector{cow_vector<int>({1, 2})});
        pub.push({3, 4});
        run_flows();
        dispatch_messages();
        check_eq(snk->buf.size(), 1u);
        advance_time(1s);
        dispatch_messages();
        check_eq(snk->buf.size(), 1u);
        snk->parent()->schedule_fn([snk] { snk->request(1); });
        run_flows();
        dispatch_messages();
        check_eq(snk->buf.size(), 2u);
        check_eq(snk->buf.back(), cow_vector<int>({3}));
        sub_hdl->dispose();
        run_flows();
        dispatch_messages();
        advance_time(1s);
        dispatch_messages();
        run_flows();
        check(snk->aborted());
        check_eq(snk->err, sec::disposed);
      }
    }
  }
}

TEST("forced batches resume before pulling more items") {
  using sub_type = caf::flow::op::buffer_sub<noskip_trait>;
  auto snk = make_passive_observer<cow_vector<int>>();
  auto input = make_counting_sub();
  auto control = make_counting_sub();
  auto uut = make_counted<sub_type>(coordinator(), 3, snk->as_observer());
  uut->fwd_on_subscribe(fwd_data, caf::flow::subscription{input});
  uut->fwd_on_subscribe(fwd_ctrl, caf::flow::subscription{control});
  snk->on_subscribe(caf::flow::subscription{uut});
  check_eq(input->demand, 0u);
  SECTION("partial batches") {
    // Spend demand on a partial batch, leaving two items in flight.
    snk->request(1);
    check_eq(input->demand, 3u);
    uut->fwd_on_next(fwd_data, 1);
    uut->fwd_on_next(fwd_ctrl, 1);
    uut->fwd_on_next(fwd_data, 2);
    uut->fwd_on_next(fwd_ctrl, 2);
    check(uut->can_emit());
    // Resume the forced batch without requesting more source items.
    snk->request(1);
    check_eq(input->demand, 3u);
    run_flows();
    check_eq(snk->buf, std::vector{cow_vector<int>{1}, cow_vector<int>{2}});
    check(!uut->can_emit());
    check_eq(input->demand, 3u);
    // Account for the remaining in-flight item when filling the next batch.
    snk->request(1);
    check_eq(input->demand, 5u);
    uut->fwd_on_next(fwd_data, 3);
    uut->fwd_on_next(fwd_data, 4);
    uut->fwd_on_next(fwd_data, 5);
    check_eq(snk->buf.back(), cow_vector<int>({3, 4, 5}));
  }
  SECTION("empty batches leave a full batch in flight") {
    snk->request(1);
    check_eq(input->demand, 3u);
    uut->fwd_on_next(fwd_ctrl, 1);
    uut->fwd_on_next(fwd_ctrl, 2);
    require(uut->can_emit());
    snk->request(1);
    check_eq(input->demand, 3u);
    run_flows();
    check_eq(snk->buf, std::vector{cow_vector<int>{}, cow_vector<int>{}});
    check(!uut->can_emit());
    // Both empty batches spent demand without consuming any source items.
    uut->fwd_on_next(fwd_data, 1);
    uut->fwd_on_next(fwd_data, 2);
    uut->fwd_on_next(fwd_data, 3);
    check_eq(uut->pending(), 3u);
    check_eq(snk->buf.size(), 2u);
    snk->request(1);
    run_flows();
    check_eq(snk->buf, std::vector{cow_vector<int>{}, cow_vector<int>{},
                                   cow_vector<int>{1, 2, 3}});
    check_eq(input->demand, 3u);
  }
  snk->unsubscribe();
  run_flows();
}

TEST("cancellation stops pending and final batches") {
  SECTION("a forced batch is pending") {
    auto snk = make_passive_observer<cow_vector<int>>();
    auto uut = raw_sub(3, make_observable().never<int>(),
                       make_observable().never<int64_t>(), snk->as_observer());
    // Exhaust demand, then receive and force items that were already in flight.
    snk->request(1);
    uut->fwd_on_next(fwd_ctrl, 1);
    uut->fwd_on_next(fwd_data, 1);
    uut->fwd_on_next(fwd_data, 2);
    uut->fwd_on_next(fwd_ctrl, 2);
    require_eq(uut->pending(), 2u);
    require(uut->can_emit());
    SECTION("the observer cancels a queued flush") {
      auto before = pending_actions();
      snk->request(1);
      require_eq(pending_actions(), before + 1);
      snk->unsubscribe();
      run_flows();
      check(snk->idle());
    }
    SECTION("external disposal overrides a deferred terminal event") {
      uut->fwd_on_complete(fwd_data);
      require(!uut->running());
      require_eq(uut->pending(), 2u);
      require(snk->subscribed());
      uut->dispose();
      run_flows();
      check(snk->aborted());
      check_eq(snk->err, sec::disposed);
    }
    check_eq(snk->buf, std::vector{cow_vector<int>{}});
    check(uut->disposed());
    check(!uut->can_emit());
  }
  SECTION("the observer cancels while receiving the final batch") {
    auto snk = make_canceling_observer<cow_vector<int>>(true, false);
    auto uut = raw_sub(3, make_observable().never<int>(),
                       make_observable().never<int64_t>(), snk->as_observer());
    uut->fwd_on_next(fwd_data, 1);
    SECTION("demand arrives before completion") {
      snk->sub.request(1);
      uut->fwd_on_complete(fwd_data);
    }
    SECTION("demand arrives after completion") {
      uut->fwd_on_complete(fwd_data);
      snk->sub.request(1);
    }
    SECTION("the control source forces the batch") {
      snk->sub.request(1);
      uut->fwd_on_next(fwd_ctrl, 1);
    }
    run_flows();
    check_eq(snk->buf, std::vector{cow_vector<int>{1}});
    check_eq(snk->on_complete_calls, 0);
    check_eq(snk->on_error_calls, 0);
    check(uut->disposed());
  }
}

TEST("buffer subscriptions keep actors alive until canceled") {
  auto snk = intrusive_ptr<passive_observer<cow_vector<int>>>{};
  auto aut = sys.spawn([&snk](event_based_actor* self) {
    snk
      = self->add_child(std::in_place_type<passive_observer<cow_vector<int>>>);
    self->make_observable().never<int>().buffer(3).subscribe(
      snk->as_observer());
  });
  dispatch_messages();
  require(snk != nullptr);
  require(!terminated(aut));
  snk->parent()->schedule_fn([snk] { snk->unsubscribe(); });
  dispatch_messages();
  check(terminated(aut));
}

SCENARIO("skip policies suppress empty batches") {
  GIVEN("a buffer operator") {
    WHEN("the control observable fires with no pending data") {
      THEN("the operator omits the batch") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub<skip_trait>(3, make_observable().never<int>(),
                                       make_observable().never<int64_t>(),
                                       snk->as_observer());
        snk->request(42);
        run_flows();
        uut->fwd_on_next(fwd_ctrl, 1);
        uut->dispose();
        run_flows();
        check(snk->buf.empty());
      }
    }
    WHEN("the control observable fires with pending data") {
      THEN("the operator emits a partial batch") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub<skip_trait>(3, make_observable().never<int>(),
                                       make_observable().never<int64_t>(),
                                       snk->as_observer());
        snk->request(42);
        run_flows();
        uut->fwd_on_next(fwd_data, 17);
        uut->fwd_on_next(fwd_ctrl, 1);
        uut->dispose();
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
        auto uut = raw_sub<noskip_trait>(3, make_observable().never<int>(),
                                         make_observable().never<int64_t>(),
                                         snk->as_observer());
        snk->request(42);
        run_flows();
        uut->fwd_on_next(fwd_ctrl, 1);
        uut->dispose();
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>()});
      }
    }
    WHEN("the control observable fires with pending data") {
      THEN("the operator emits a partial batch") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub<noskip_trait>(3, make_observable().never<int>(),
                                         make_observable().never<int64_t>(),
                                         snk->as_observer());
        snk->request(42);
        run_flows();
        uut->fwd_on_next(fwd_data, 17);
        uut->fwd_on_next(fwd_ctrl, 1);
        uut->dispose();
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
        auto uut = raw_sub<skip_trait>(3, make_observable().never<int>(),
                                       make_observable().never<int64_t>(),
                                       snk->as_observer());
        snk->request(42);
        run_flows();
        uut->dispose();
        run_flows();
        check(snk->aborted());
      }
    }
  }
}

SCENARIO("on_request actions can turn into no-ops") {
  GIVEN("a buffer operator") {
    WHEN("the sink requests more data right before a timeout triggers") {
      THEN("the batch gets shipped and the on_request action does nothing") {
        auto snk = flow::make_passive_observer<cow_vector<int>>();
        auto uut = raw_sub<skip_trait>(3, make_observable().never<int>(),
                                       make_observable().never<int64_t>(),
                                       snk->as_observer());
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
        uut->dispose();
        run_flows();
        check_eq(snk->buf, std::vector{cow_vector<int>({0, 1, 2})});
      }
    }
  }
}

SCENARIO("a buffer must have a positive period") {
  GIVEN("a buffer operator") {
    WHEN("calling .buffer(3, 0s)") {
      THEN("the operator raises an error") {
        auto err = std::make_shared<error>();
        auto num_items = std::make_shared<int>(0);
        make_observable()
          .iota(1)
          .take(100)
          .buffer(3, 0s)
          .do_on_error([err](const error& what) { *err = what; })
          .for_each([num_items](const cow_vector<int>&) { *num_items += 1; });
        dispatch_messages();
        check_eq(*num_items, 0);
        check_eq(*err, caf::sec::invalid_argument);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
