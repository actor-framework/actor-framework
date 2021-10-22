// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.meta_object

#include "caf/detail/meta_object.hpp"

#include "core-test.hpp"

#include <tuple>
#include <type_traits>

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/make_meta_object.hpp"
#include "caf/init_global_meta_objects.hpp"

using namespace std::string_literals;

using namespace caf;
using namespace caf::detail;

size_t i32_wrapper::instances = 0;

size_t i64_wrapper::instances = 0;

namespace {

struct fixture {
  fixture() {
    CAF_ASSERT(i32_wrapper::instances == 0);
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(meta objects allow construction and destruction of objects) {
  auto meta_i32_wrapper = make_meta_object<i32_wrapper>("i32_wrapper");
  std::aligned_storage_t<sizeof(i32_wrapper), alignof(i32_wrapper)> storage;
  meta_i32_wrapper.default_construct(&storage);
  CHECK_EQ(i32_wrapper::instances, 1u);
  meta_i32_wrapper.destroy(&storage);
  CHECK_EQ(i32_wrapper::instances, 0u);
}

CAF_TEST(meta objects allow serialization of objects) {
  byte_buffer buf;
  auto meta_i32_wrapper = make_meta_object<i32_wrapper>("i32_wrapper");
  std::aligned_storage_t<sizeof(i32_wrapper), alignof(i32_wrapper)> storage;
  binary_serializer sink{nullptr, buf};
  meta_i32_wrapper.default_construct(&storage);
  CHECK_EQ(i32_wrapper::instances, 1u);
  CHECK(meta_i32_wrapper.save_binary(sink, &storage));
  i32_wrapper copy;
  CHECK_EQ(i32_wrapper::instances, 2u);
  copy.value = 42;
  binary_deserializer source{nullptr, buf};
  CHECK(meta_i32_wrapper.load_binary(source, &copy));
  CHECK_EQ(copy.value, 0);
  meta_i32_wrapper.destroy(&storage);
  CHECK_EQ(i32_wrapper::instances, 1u);
}

CAF_TEST(init_global_meta_objects takes care of creating a meta object table) {
  auto xs = global_meta_objects();
  CAF_REQUIRE_EQUAL(xs.size(), caf::id_block::core_test::end);
  CHECK_EQ(type_name_by_id_v<type_id_v<i32_wrapper>>, "i32_wrapper"s);
  CHECK_EQ(type_name_by_id_v<type_id_v<i64_wrapper>>, "i64_wrapper"s);
  CHECK_EQ(xs[type_id_v<i32_wrapper>].type_name, "i32_wrapper"s);
  CHECK_EQ(xs[type_id_v<i64_wrapper>].type_name, "i64_wrapper"s);
  MESSAGE("calling init_global_meta_objects again is a no-op");
  init_global_meta_objects<id_block::core_test>();
  auto ys = global_meta_objects();
  auto same = [](const auto& x, const auto& y) {
    return x.type_name == y.type_name;
  };
  CHECK(std::equal(xs.begin(), xs.end(), ys.begin(), ys.end(), same));
}

END_FIXTURE_SCOPE()
