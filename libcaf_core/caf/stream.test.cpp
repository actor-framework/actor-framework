// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/stream.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/log/test.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/system_messages.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using ivec = std::vector<int>;

behavior int_sink(event_based_actor* self, std::shared_ptr<ivec> results,
                  std::shared_ptr<error> err) {
  log::test::debug("stream.test", "started sink with ID {}", self->id());
  return {
    [self, results, err](const stream& input) {
      self //
        ->observe_as<int>(input, 30, 10)
        .do_on_error([self, err](const error& what) {
          log::test::debug("stream.test", "sink with ID {} got error: {}",
                           self->id(), what);
          *err = what;
        })
        .do_finally([self] {
          log::test::debug("stream.test", "sink with ID {} shuts down",
                           self->id());
          self->quit();
        })
        .for_each([results](int x) { results->push_back(x); });
    },
  };
}

} // namespace

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

TEST("streams are comparable") {
  auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
  auto dummy_guard = make_actor_scope_guard(dummy);
  auto uut = stream{actor_cast<strong_actor_ptr>(dummy), type_id_v<int32_t>,
                    "foo", 42};
  check_eq(uut.compare(uut), 0);
  auto uut2 = stream{actor_cast<strong_actor_ptr>(dummy), type_id_v<int32_t>,
                     "foo", 43};
  check_eq(uut.compare(uut2), -1);
  auto uut3 = stream{actor_cast<strong_actor_ptr>(dummy), type_id_v<int32_t>,
                     "foo", 41};
  check_eq(uut.compare(uut3), 1);
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

TEST("streams allow actors to transmit flow items to other actors") {
  auto res = ivec{};
  res.resize(256);
  std::iota(res.begin(), res.end(), 1);
  auto r1 = std::make_shared<ivec>();
  auto e1 = std::make_shared<error>();
  auto s1 = sys.spawn(int_sink, r1, e1);
  auto r2 = std::make_shared<ivec>();
  auto e2 = std::make_shared<error>();
  auto s2 = sys.spawn(int_sink, r2, e2);
  SECTION("streams may be subscribed to multiple times") {
    auto src = sys.spawn([s1, s2](event_based_actor* self) {
      auto vals = self //
                    ->make_observable()
                    .iota(1)
                    .take(256)
                    .to_stream("foo", 10ms, 10);
      self->mail(vals).send(s1);
      self->mail(vals).send(s2);
    });
    expect<stream>().from(src).to(s1);
    expect<stream>().from(src).to(s2);
    expect<stream_open_msg>().from(s1).to(src);
    expect<stream_open_msg>().from(s2).to(src);
    expect<stream_ack_msg>().from(src).to(s1);
    expect<stream_ack_msg>().from(src).to(s2);
    dispatch_messages();
    check_eq(*r1, res);
    check(!*e1);
    check_eq(*r2, res);
    check(!*e2);
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
      self->mail(vals).send(s1);
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
      self->mail(vals).send(s1);
    });
    expect<stream>().from(src).to(s1);
    expect<stream_open_msg>().from(s1).to(src);
    expect<stream_ack_msg>().from(src).to(s1);
    inject_exit(s1);
    check(terminated(s1));
    prepone_and_expect<stream_cancel_msg>().to(src);
    check(!terminated(src)); // Must clean up state but not terminate.
  }
  SECTION("streams must have a positive delay") {
    auto src = sys.spawn([s1](event_based_actor* self) {
      auto vals = self //
                    ->make_observable()
                    .iota(1)
                    .take(256)
                    .to_stream("foo", 0ms, 10);
      self->mail(vals).send(s1);
    });
    expect<stream>().from(src).to(s1);
    expect<stream_open_msg>().from(s1).to(src);
    expect<stream_abort_msg>().from(src).to(s1);
    dispatch_messages();
    check(terminated(s1));
    check_eq(*r1, ivec{});
    check_eq(*e1, sec::invalid_argument);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)
