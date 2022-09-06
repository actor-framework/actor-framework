// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE stream

#include "caf/stream.hpp"

#include "core-test.hpp"

using namespace caf;

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

END_FIXTURE_SCOPE()
