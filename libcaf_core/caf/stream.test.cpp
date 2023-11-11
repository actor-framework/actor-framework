// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/stream.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using ivec = std::vector<int>;

constexpr size_t max_batch_size = 10;

behavior int_sink(event_based_actor* self, std::shared_ptr<ivec> results) {
  caf::logger::debug("stream.test", "started sink with ID {}", self->id());
  return {
    [self, results](const stream& input) {
      self //
        ->observe_as<int>(input, 30, 10)
        .do_finally([self] {
          caf::logger::debug("stream.test", "sink with ID {} shuts down",
                             self->id());
          self->quit();
        })
        .for_each([results](int x) { results->push_back(x); });
    },
  };
}

WITH_FIXTURE(test::fixture::deterministic) {

TEST("default-constructed streams are invalid") {
  auto uut = stream{};
  check(!uut.has_element_type<int32_t>());
  check_eq(uut.id(), 0u);
  check_eq(uut.type(), invalid_type_id);
  check_eq(uut.name(), "");
  check_eq(uut.source(), nullptr);
  check_eq(uut, serialization_roundtrip(uut));
}

TEST("streams are serializable") {
  auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
  // Note: we need to terminate the dummy actor manually because the registry
  //       holds a reference to it when we serialize it.
  auto dummy_guard = make_actor_scope_guard(dummy);
  auto uut = stream{actor_cast<strong_actor_ptr>(dummy), type_id_v<int32_t>,
                    "foo", 42};
  check(uut.has_element_type<int32_t>());
  check_eq(uut.id(), 42u);
  check_eq(uut.type(), type_id_v<int32_t>);
  check_eq(uut.name(), "foo");
  check_eq(uut.source(), dummy);
  check_eq(uut, serialization_roundtrip(uut));
}

TEST("streams are comparable") {
  auto dummy1 = sys.spawn([] { return behavior{[](int) {}}; });
  auto dummy2 = sys.spawn([] { return behavior{[](int) {}}; });
  stream s1;
  stream s2;
  if (dummy1 > dummy2) {
    std::swap(dummy1, dummy2);
  }
  SECTION("streams with different sources are not equal") {
    s1 = stream{actor_cast<strong_actor_ptr>(dummy1), type_id_v<int32_t>, "foo",
                42};
    s2 = stream{actor_cast<strong_actor_ptr>(dummy2), type_id_v<int32_t>, "foo",
                42};
    check_ne(s1, s2);
    check_lt(s1, s2);
    check_le(s1, s2);
    check_gt(s2, s1);
    check_ge(s2, s1);
  }
  SECTION("streams with the same source and ID are equal") {
    s1 = stream{actor_cast<strong_actor_ptr>(dummy1), type_id_v<int32_t>, "foo",
                42};
    s2 = stream{actor_cast<strong_actor_ptr>(dummy1), type_id_v<int32_t>, "foo",
                42};
    check_eq(s1, s2);
    check_le(s1, s2);
    check_ge(s2, s1);
  }
  SECTION("streams to the same source are sorted by ID") {
    s1 = stream{actor_cast<strong_actor_ptr>(dummy1), type_id_v<int32_t>, "foo",
                42};
    s2 = stream{actor_cast<strong_actor_ptr>(dummy1), type_id_v<int32_t>, "bar",
                84};
    check_ne(s1, s2);
    check_lt(s1, s2);
    check_le(s1, s2);
    check_gt(s2, s1);
    check_ge(s2, s1);
  }
}

TEST("streams allow actors to transmit flow items to other actors") {
  auto res = ivec{};
  res.resize(256);
  std::iota(res.begin(), res.end(), 1);
  auto r1 = std::make_shared<ivec>();
  auto s1 = sys.spawn(int_sink, r1);
  auto r2 = std::make_shared<ivec>();
  auto s2 = sys.spawn(int_sink, r2);
  SECTION("streams may be subscribed to multiple times") {
    auto src = sys.spawn([s1, s2](event_based_actor* self) {
      auto vals = self //
                    ->make_observable()
                    .iota(1)
                    .take(256)
                    .to_stream("foo", 10ms, 10);
      self->send(s1, vals);
      self->send(s2, vals);
    });
    expect<stream>().from(src).to(s1);
    expect<stream>().from(src).to(s2);
    expect<stream_open_msg>().from(s1).to(src);
    expect<stream_open_msg>().from(s2).to(src);
    expect<stream_ack_msg>().from(src).to(s1);
    expect<stream_ack_msg>().from(src).to(s2);
    dispatch_messages();
    check_eq(*r1, res);
    check_eq(*r2, res);
    check(terminated(s1));
    check(terminated(s2));
  }
  SECTION("stream sources terminates open streams when shutting down") {
    inject_exit(s2);
    auto src = sys.spawn([s1](event_based_actor* self) {
      auto vals = self //
                    ->make_observable()
                    .iota(1)
                    .take(256)
                    .to_stream("foo", 10ms, 10);
      self->send(s1, vals);
    });
    expect<stream>().from(src).to(s1);
    expect<stream_open_msg>().from(s1).to(src);
    expect<stream_ack_msg>().from(src).to(s1);
    inject_exit(src);
    check(terminated(src));
    prepone_and_expect<stream_abort_msg>().to(s1);
    check(terminated(s1));
  }
  SECTION("stream sinks cancel open subscriptions when shutting down") {
    inject_exit(s2);
    auto src = sys.spawn([s1](event_based_actor* self) {
      auto vals = self //
                    ->make_observable()
                    .iota(1)
                    .take(256)
                    .to_stream("foo", 10ms, 10);
      self->send(s1, vals);
    });
    expect<stream>().from(src).to(s1);
    expect<stream_open_msg>().from(s1).to(src);
    expect<stream_ack_msg>().from(src).to(s1);
    inject_exit(s1);
    check(terminated(s1));
    prepone_and_expect<stream_cancel_msg>().to(src);
    check(!terminated(src)); // Must clean up state but not terminate.
  }
}

SCENARIO("actors transparently translate between stream messages and flows") {
  GIVEN("a stream source that produces up to 256 items") {
    auto vals = std::make_shared<stream>();
    auto uut = sys.spawn([vals](event_based_actor* self) {
      *vals = self //
                ->make_observable()
                .iota(1)
                .take(256)
                .to_stream("foo", 10ms, max_batch_size);
    });
    require_ne(vals, nullptr);
    auto dummy = make_null_actor();
    auto dummy_ptr = actor_cast<strong_actor_ptr>(dummy);
    WHEN("the consumer sends the open handshake as anonymous message") {
      THEN("the message has no effect") {
        anon_send(uut, stream_open_msg{vals->id(), dummy_ptr, 42});
        expect<stream_open_msg>().to(uut);
        // Note: the only thing we could maybe check is that the actor logs an
        //       error message. However, there's no API for that at the moment.
      }
    }
    WHEN("the consumer requests a non-existing ID") {
      THEN("the source responds with an abort message") {
        inject()
          .with(stream_open_msg{vals->id() + 1, dummy_ptr, 42})
          .from(dummy)
          .to(uut);
        expect<stream_abort_msg>()
          .with([](const stream_abort_msg& msg) {
            return msg.sink_flow_id == 42 && msg.reason == sec::invalid_stream;
          })
          .from(uut)
          .to(dummy);
      }
    }
    WHEN("the consumer requests an existing ID") {
      THEN("the source responds with an ack message containing the flow ID") {
        inject()
          .with(stream_open_msg{vals->id(), dummy_ptr, 42})
          .from(dummy)
          .to(uut);
        auto flow_id = uint64_t{0};
        auto max_items_per_batch = uint32_t{0};
        expect<stream_ack_msg>()
          .with([&](const stream_ack_msg& msg) {
            flow_id = msg.source_flow_id;
            max_items_per_batch = msg.max_items_per_batch;
            return true;
          })
          .from(uut)
          .to(dummy);
        check_gt(flow_id, 0u);
        inject().with(stream_cancel_msg{flow_id}).from(dummy).to(uut);
      }
    }
    WHEN("the consumer sends credit for consuming all items at once") {
      THEN("the source responds all batches followed by a close message") {
        inject()
          .with(stream_open_msg{vals->id(), dummy_ptr, 42})
          .from(dummy)
          .to(uut);
        auto flow_id = uint64_t{0};
        auto max_items_per_batch = uint32_t{0};
        expect<stream_ack_msg>()
          .with([&](const stream_ack_msg& msg) {
            flow_id = msg.source_flow_id;
            max_items_per_batch = msg.max_items_per_batch;
            return true;
          })
          .from(uut)
          .to(dummy);
        check_gt(flow_id, 0u);
        auto total = size_t{0};
        auto pred = [&total](const stream_batch_msg& msg) {
          total += msg.content.size();
          return true;
        };
        inject().with(stream_demand_msg{flow_id, 50}).from(dummy).to(uut);
        while (allow<stream_batch_msg>().with(pred).from(uut).to(dummy)) {
          // repeat
        }
        check_eq(total, 256u);
        expect<stream_close_msg>().from(uut).to(dummy);
      }
    }
    WHEN("the consumer sends credit consuming only a subset at a time") {
      THEN("the source uses up the credit before waiting for more demand") {
        inject()
          .with(stream_open_msg{vals->id(), dummy_ptr, 42})
          .from(dummy)
          .to(uut);
        auto flow_id = uint64_t{0};
        auto max_items_per_batch = uint32_t{0};
        expect<stream_ack_msg>()
          .with([&](const stream_ack_msg& msg) {
            flow_id = msg.source_flow_id;
            max_items_per_batch = msg.max_items_per_batch;
            return true;
          })
          .from(uut)
          .to(dummy);
        check_gt(flow_id, 0u);
        auto total = size_t{0};
        auto pred = [this, &total](const stream_batch_msg& msg) {
          check_le(msg.content.size(), max_batch_size);
          total += msg.content.size();
          return true;
        };
        // Pull 10 items (one batch).
        inject().with(stream_demand_msg{flow_id, 1}).from(dummy).to(uut);
        expect<stream_batch_msg>().with(pred).from(uut).to(dummy);
        check_eq(mail_count(), 0u);
        check_eq(total, 10u);
        // Pull 20 more items (two batches).
        inject().with(stream_demand_msg{flow_id, 2}).from(dummy).to(uut);
        expect<stream_batch_msg>().with(pred).from(uut).to(dummy);
        expect<stream_batch_msg>().with(pred).from(uut).to(dummy);
        check_eq(mail_count(), 0u);
        check_eq(total, 30u);
        // Pull the remaining items.
        inject().with(stream_demand_msg{flow_id, 50}).from(dummy).to(uut);
        while (allow<stream_batch_msg>().with(pred).from(uut).to(dummy)) {
          // repeat
        }
        check_eq(mail_count(), 1u);
        check_eq(total, 256u);
        // Source must close the stream.
        expect<stream_close_msg>().from(uut).to(dummy);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
