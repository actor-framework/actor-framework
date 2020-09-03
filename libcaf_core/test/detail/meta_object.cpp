/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

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

CAF_TEST_FIXTURE_SCOPE(meta_object_tests, fixture)

CAF_TEST(meta objects allow construction and destruction of objects) {
  auto meta_i32_wrapper = make_meta_object<i32_wrapper>("i32_wrapper");
  std::aligned_storage_t<sizeof(i32_wrapper), alignof(i32_wrapper)> storage;
  meta_i32_wrapper.default_construct(&storage);
  CAF_CHECK_EQUAL(i32_wrapper::instances, 1u);
  meta_i32_wrapper.destroy(&storage);
  CAF_CHECK_EQUAL(i32_wrapper::instances, 0u);
}

CAF_TEST(meta objects allow serialization of objects) {
  byte_buffer buf;
  auto meta_i32_wrapper = make_meta_object<i32_wrapper>("i32_wrapper");
  std::aligned_storage_t<sizeof(i32_wrapper), alignof(i32_wrapper)> storage;
  binary_serializer sink{nullptr, buf};
  meta_i32_wrapper.default_construct(&storage);
  CAF_CHECK_EQUAL(i32_wrapper::instances, 1u);
  CAF_CHECK(meta_i32_wrapper.save_binary(sink, &storage));
  i32_wrapper copy;
  CAF_CHECK_EQUAL(i32_wrapper::instances, 2u);
  copy.value = 42;
  binary_deserializer source{nullptr, buf};
  CAF_CHECK(meta_i32_wrapper.load_binary(source, &copy));
  CAF_CHECK_EQUAL(copy.value, 0);
  meta_i32_wrapper.destroy(&storage);
  CAF_CHECK_EQUAL(i32_wrapper::instances, 1u);
}

CAF_TEST(init_global_meta_objects takes care of creating a meta object table) {
  auto xs = global_meta_objects();
  CAF_REQUIRE_EQUAL(xs.size(), caf::id_block::core_test::end);
  CAF_CHECK_EQUAL(type_name_by_id_v<type_id_v<i32_wrapper>>, "i32_wrapper"s);
  CAF_CHECK_EQUAL(type_name_by_id_v<type_id_v<i64_wrapper>>, "i64_wrapper"s);
  CAF_CHECK_EQUAL(xs[type_id_v<i32_wrapper>].type_name, "i32_wrapper"s);
  CAF_CHECK_EQUAL(xs[type_id_v<i64_wrapper>].type_name, "i64_wrapper"s);
  CAF_MESSAGE("calling init_global_meta_objects again is a no-op");
  init_global_meta_objects<id_block::core_test>();
  auto ys = global_meta_objects();
  auto same = [](const auto& x, const auto& y) {
    return x.type_name == y.type_name;
  };
  CAF_CHECK(std::equal(xs.begin(), xs.end(), ys.begin(), ys.end(), same));
}

CAF_TEST_FIXTURE_SCOPE_END()
