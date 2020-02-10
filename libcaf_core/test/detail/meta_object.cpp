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

#include "caf/test/dsl.hpp"

#include <tuple>
#include <type_traits>

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/make_meta_object.hpp"
#include "caf/init_global_meta_objects.hpp"

namespace {

struct i32_wrapper;
struct i64_wrapper;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(meta_object_tests, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(meta_object_tests, i32_wrapper)

  CAF_ADD_TYPE_ID(meta_object_tests, i64_wrapper)

CAF_END_TYPE_ID_BLOCK(meta_object_tests)

using namespace std::string_literals;

using namespace caf;
using namespace caf::detail;

static_assert(meta_object_tests_first_type_id == first_custom_type_id);

static_assert(meta_object_tests_last_type_id == first_custom_type_id + 1);

static_assert(type_id_v<i32_wrapper> == first_custom_type_id);

static_assert(type_id_v<i64_wrapper> == first_custom_type_id + 1);

static_assert(type_id_v<i32_wrapper> == meta_object_tests_first_type_id);

static_assert(type_id_v<i64_wrapper> == meta_object_tests_last_type_id);

namespace {

struct i32_wrapper {
  static size_t instances;

  int32_t value;

  i32_wrapper() : value(0) {
    ++instances;
  }

  ~i32_wrapper() {
    --instances;
  }
};

template <class Inspector>
auto inspect(Inspector& f, i32_wrapper& x) {
  return f(x.value);
}

size_t i32_wrapper::instances = 0;

struct i64_wrapper {
  static size_t instances;

  int64_t value;

  i64_wrapper() : value(0) {
    ++instances;
  }

  ~i64_wrapper() {
    --instances;
  }
};

template <class Inspector>
auto inspect(Inspector& f, i64_wrapper& x) {
  return f(x.value);
}

size_t i64_wrapper::instances = 0;

struct fixture {
  fixture() {
    CAF_ASSERT(i32_wrapper::instances == 0);
    clear_global_meta_objects();
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
  meta_i32_wrapper.save_binary(sink, &storage);
  i32_wrapper copy;
  CAF_CHECK_EQUAL(i32_wrapper::instances, 2u);
  copy.value = 42;
  binary_deserializer source{nullptr, buf};
  meta_i32_wrapper.load_binary(source, &copy);
  CAF_CHECK_EQUAL(copy.value, 0);
  meta_i32_wrapper.destroy(&storage);
  CAF_CHECK_EQUAL(i32_wrapper::instances, 1u);
}

CAF_TEST(resizing the global meta objects keeps entries) {
  auto eq = [](const meta_object& x, const meta_object& y) {
    return std::tie(x.destroy, x.default_construct, x.save_binary,
                    x.load_binary, x.save, x.load)
           == std::tie(y.destroy, y.default_construct, y.save_binary,
                       y.load_binary, y.save, y.load);
  };
  auto meta_i32 = make_meta_object<int32_t>("int32_t");
  auto xs1 = resize_global_meta_objects(1);
  CAF_CHECK_EQUAL(xs1.size(), 1u);
  xs1[0] = meta_i32;
  CAF_CHECK(eq(xs1[0], meta_i32));
  auto xs2 = resize_global_meta_objects(2);
  CAF_CHECK_EQUAL(xs2.size(), 2u);
  CAF_CHECK(eq(xs2[0], meta_i32));
  resize_global_meta_objects(3);
  CAF_CHECK(eq(global_meta_object(0), meta_i32));
}

CAF_TEST(init_global_meta_objects takes care of creating a meta object table) {
  detail::clear_global_meta_objects();
  init_global_meta_objects<meta_object_tests_type_ids>();
  auto xs = global_meta_objects();
  CAF_REQUIRE_EQUAL(xs.size(), meta_object_tests_last_type_id + 1u);
  CAF_CHECK_EQUAL(type_name_by_id_v<type_id_v<i32_wrapper>>, "i32_wrapper"s);
  CAF_CHECK_EQUAL(type_name_by_id_v<type_id_v<i64_wrapper>>, "i64_wrapper"s);
  CAF_CHECK_EQUAL(xs[type_id_v<i32_wrapper>].type_name, "i32_wrapper"s);
  CAF_CHECK_EQUAL(xs[type_id_v<i64_wrapper>].type_name, "i64_wrapper"s);
  CAF_MESSAGE("calling init_global_meta_objects again is a no-op");
  init_global_meta_objects<meta_object_tests_type_ids>();
  auto ys = global_meta_objects();
  auto same = [](const auto& x, const auto& y) {
    return x.type_name == y.type_name;
  };
  CAF_CHECK(std::equal(xs.begin(), xs.end(), ys.begin(), ys.end(), same));
  CAF_MESSAGE("init_global_meta_objects can initialize allocated chunks");
  CAF_CHECK_EQUAL(global_meta_object(type_id_v<float>).type_name, nullptr);
  init_global_meta_objects<builtin_type_ids>();
  CAF_MESSAGE("again, calling init_global_meta_objects again is a no-op");
  init_global_meta_objects<builtin_type_ids>();
  CAF_CHECK_EQUAL(global_meta_object(type_id_v<float>).type_name, "float"s);
}

CAF_TEST_FIXTURE_SCOPE_END()
