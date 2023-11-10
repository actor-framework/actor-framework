// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/typed_stream.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;
using namespace std::literals;

// Some offset to avoid collisions with other type IDs.
CAF_BEGIN_TYPE_ID_BLOCK(typed_stream_test, caf::first_custom_type_id + 20)
  CAF_ADD_TYPE_ID(typed_stream_test, (caf::typed_stream<int32_t>) )
CAF_END_TYPE_ID_BLOCK(typed_stream_test)

namespace {

using ivec = std::vector<int>;

using stream_type = typed_stream<int32_t>;

behavior int_sink(event_based_actor* self, std::shared_ptr<ivec> results) {
  caf::logger::debug("stream.test", "started sink with ID {}", self->id());
  return {
    [self, results](const stream_type& input) {
      self //
        ->observe(input, 30, 10)
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
  auto uut = stream_type{};
  check_eq(uut.id(), 0u);
  check_eq(uut.source(), nullptr);
  check_eq(uut, serialization_roundtrip(uut));
}

TEST("streams are serializable") {
  auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
  // Note: we need to terminate the dummy actor manually because the registry
  //       holds a reference to it when we serialize it.
  auto dummy_guard = make_actor_scope_guard(dummy);
  auto uut = stream_type{actor_cast<strong_actor_ptr>(dummy), "foo", 42};
  check_eq(uut.id(), 42u);
  check_eq(uut.name(), "foo");
  check_eq(uut.source(), dummy);
  check_eq(uut, serialization_roundtrip(uut));
}

TEST("streams are comparable") {
  auto dummy1 = sys.spawn([] { return behavior{[](int) {}}; });
  auto dummy2 = sys.spawn([] { return behavior{[](int) {}}; });
  stream_type s1;
  stream_type s2;
  if (dummy1 > dummy2) {
    std::swap(dummy1, dummy2);
  }
  SECTION("streams with different sources are not equal") {
    s1 = stream_type{actor_cast<strong_actor_ptr>(dummy1), "foo", 42};
    s2 = stream_type{actor_cast<strong_actor_ptr>(dummy2), "foo", 42};
    check_ne(s1, s2);
    check_lt(s1, s2);
    check_le(s1, s2);
    check_gt(s2, s1);
    check_ge(s2, s1);
  }
  SECTION("streams with the same source and ID are equal") {
    s1 = stream_type{actor_cast<strong_actor_ptr>(dummy1), "foo", 42};
    s2 = stream_type{actor_cast<strong_actor_ptr>(dummy1), "foo", 42};
    check_eq(s1, s2);
    check_le(s1, s2);
    check_ge(s2, s1);
  }
  SECTION("streams to the same source are sorted by ID") {
    s1 = stream_type{actor_cast<strong_actor_ptr>(dummy1), "foo", 42};
    s2 = stream_type{actor_cast<strong_actor_ptr>(dummy1), "bar", 84};
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
                    .iota(int32_t{1})
                    .take(256)
                    .to_typed_stream("foo", 10ms, 10);
      self->send(s1, vals);
      self->send(s2, vals);
    });
    expect<stream_type>().from(src).to(s1);
    expect<stream_type>().from(src).to(s2);
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
                    .iota(int32_t{1})
                    .take(256)
                    .to_typed_stream("foo", 10ms, 10);
      self->send(s1, vals);
    });
    expect<stream_type>().from(src).to(s1);
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
                    .iota(int32_t{1})
                    .take(256)
                    .to_typed_stream("foo", 10ms, 10);
      self->send(s1, vals);
    });
    expect<stream_type>().from(src).to(s1);
    expect<stream_open_msg>().from(s1).to(src);
    expect<stream_ack_msg>().from(src).to(s1);
    inject_exit(s1);
    check(terminated(s1));
    prepone_and_expect<stream_cancel_msg>().to(src);
    check(!terminated(src)); // Must clean up state but not terminate.
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

TEST_INIT() {
  init_global_meta_objects<id_block::typed_stream_test>();
}

} // namespace
