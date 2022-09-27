// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE stream

#include "caf/stream.hpp"

#include "core-test.hpp"

#include "caf/scheduled_actor/flow.hpp"

using namespace caf;
using namespace std::literals;

namespace {

struct fixture : test_coordinator_fixture<> {
  template <class T>
  caf::expected<T> deep_copy(const T& obj) {
    caf::byte_buffer buf;
    {
      caf::binary_serializer sink{sys, buf};
      if (!sink.apply(obj))
        return {sink.get_error()};
    }
    auto result = T{};
    {
      caf::binary_deserializer source{sys, buf};
      if (!source.apply(result))
        return {source.get_error()};
    }
    return result;
  }
};

using ivec = std::vector<int>;

behavior int_sink(event_based_actor* self, std::shared_ptr<ivec> results) {
  return {
    [self, results](stream input) {
      self //
        ->observe_as<int>(input, 30, 10)
        .for_each([results](int x) { results->push_back(x); });
    },
  };
}

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

TEST_CASE("default-constructed") {
  auto uut = stream{};
  CHECK(!uut.has_element_type<int32_t>());
  CHECK_EQ(uut.id(), 0u);
  CHECK_EQ(uut.type(), invalid_type_id);
  CHECK_EQ(uut.name(), "");
  CHECK_EQ(uut.source(), nullptr);
  CHECK_EQ(uut, deep_copy(uut));
}

TEST_CASE("value-constructed") {
  auto dummy = sys.spawn([] {});
  auto uut = stream{actor_cast<strong_actor_ptr>(dummy), type_id_v<int32_t>,
                    "foo", 42};
  CHECK(uut.has_element_type<int32_t>());
  CHECK_EQ(uut.id(), 42u);
  CHECK_EQ(uut.type(), type_id_v<int32_t>);
  CHECK_EQ(uut.name(), "foo");
  CHECK_EQ(uut.source(), dummy);
  CHECK_EQ(uut, deep_copy(uut));
}

TEST_CASE("streams allow actors to transmit flow items to others") {
  auto res = ivec{};
  res.resize(256);
  std::iota(res.begin(), res.end(), 1);
  auto r1 = std::make_shared<ivec>();
  auto s1 = sys.spawn(int_sink, r1);
  auto r2 = std::make_shared<ivec>();
  auto s2 = sys.spawn(int_sink, r2);
  run();
  auto src = sys.spawn([s1, s2](event_based_actor* self) {
    auto vals = self //
                  ->make_observable()
                  .iota(1)
                  .take(256)
                  .compose(self->to_stream("foo", 10ms, 10));
    self->send(s1, vals);
    self->send(s2, vals);
  });
  run_once();
  expect((stream), from(src).to(s1));
  expect((stream), from(src).to(s2));
  expect((stream_open_msg), from(s1).to(src));
  expect((stream_open_msg), from(s2).to(src));
  expect((stream_ack_msg), from(src).to(s1));
  expect((stream_ack_msg), from(src).to(s2));
  run();
  CHECK_EQ(*r1, res);
  CHECK_EQ(*r2, res);
}

END_FIXTURE_SCOPE()
