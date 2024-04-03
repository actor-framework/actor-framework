// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/typed_stream.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_registry.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;
using namespace std::literals;

CAF_BEGIN_TYPE_ID_BLOCK(typed_stream_test, caf::first_custom_type_id + 115)

  CAF_ADD_TYPE_ID(typed_stream_test, (caf::typed_stream<int32_t>) )

CAF_END_TYPE_ID_BLOCK(typed_stream_test)

namespace {

struct fixture : test::fixture::deterministic {
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

using ivec = std::vector<int32_t>;

using istream = typed_stream<int32_t>;

behavior int_sink(event_based_actor* self, std::shared_ptr<ivec> results) {
  return {
    [self, results](istream input) {
      self //
        ->observe(input, 30, 10)
        .for_each([results](int x) { results->push_back(x); });
    },
  };
}

WITH_FIXTURE(fixture) {

TEST("default-constructed") {
  auto uut = istream{};
  check_eq(uut.id(), 0u);
  check_eq(uut.name(), "");
  check_eq(uut.source(), nullptr);
  check_eq(uut, deep_copy(uut));
}

TEST("value-constructed") {
  auto dummy = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [=](const std::string&) {},
    };
  });
  auto uut = istream{actor_cast<strong_actor_ptr>(dummy), "foo", 42};
  check_eq(uut.id(), 42u);
  check_eq(uut.name(), "foo");
  check_eq(uut.source(), dummy);
  check_eq(uut, deep_copy(uut));
  anon_send_exit(dummy, exit_reason::user_shutdown);
  dispatch_messages();
}

TEST("streams allow actors to transmit flow items to others") {
  auto res = ivec{};
  res.resize(256);
  std::iota(res.begin(), res.end(), 1);
  auto r1 = std::make_shared<ivec>();
  auto s1 = sys.spawn(int_sink, r1);
  auto r2 = std::make_shared<ivec>();
  auto s2 = sys.spawn(int_sink, r2);
  dispatch_messages();
  auto src = sys.spawn([s1, s2](event_based_actor* self) {
    auto vals = self //
                  ->make_observable()
                  .iota(int32_t{1})
                  .take(256)
                  .to_typed_stream("foo", 10ms, 10);
    self->mail(vals).send(s1);
    self->mail(vals).send(s2);
  });
  expect<istream>().from(src).to(s1);
  expect<istream>().from(src).to(s2);
  expect<stream_open_msg>().from(s1).to(src);
  expect<stream_open_msg>().from(s2).to(src);
  expect<stream_ack_msg>().from(src).to(s1);
  expect<stream_ack_msg>().from(src).to(s2);
  dispatch_messages();
  check_eq(*r1, res);
  check_eq(*r2, res);
}

} // WITH_FIXTURE(fixture)

TEST_INIT() {
  init_global_meta_objects<id_block::typed_stream_test>();
}

} // namespace
