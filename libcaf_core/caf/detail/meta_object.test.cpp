// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/meta_object.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/make_meta_object.hpp"
#include "caf/init_global_meta_objects.hpp"

#include <tuple>
#include <type_traits>

struct i32_wrapper;
struct i64_wrapper;

CAF_BEGIN_TYPE_ID_BLOCK(meta_object_test, caf::first_custom_type_id)
  CAF_ADD_TYPE_ID(meta_object_test, (i32_wrapper))
  CAF_ADD_TYPE_ID(meta_object_test, (i64_wrapper))
CAF_END_TYPE_ID_BLOCK(meta_object_test)

using namespace std::string_literals;

using namespace caf;
using namespace caf::detail;

struct i32_wrapper {
  // Initialized in meta_object.cpp.
  static size_t instances;

  int32_t value;

  i32_wrapper() : value(0) {
    ++instances;
  }

  ~i32_wrapper() {
    --instances;
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, i32_wrapper& x) {
    return f.apply(x.value);
  }
};

struct i64_wrapper {
  // Initialized in meta_object.cpp.
  static size_t instances;

  int64_t value;

  i64_wrapper() : value(0) {
    ++instances;
  }

  explicit i64_wrapper(int64_t val) : value(val) {
    ++instances;
  }

  ~i64_wrapper() {
    --instances;
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, i64_wrapper& x) {
    return f.apply(x.value);
  }
};

size_t i32_wrapper::instances = 0;

size_t i64_wrapper::instances = 0;

TEST("meta objects allow construction and destruction of objects") {
  auto meta_i32_wrapper = make_meta_object<i32_wrapper>("i32_wrapper");
  alignas(i32_wrapper) std::byte storage[sizeof(i32_wrapper)];
  meta_i32_wrapper.default_construct(&storage);
  check_eq(i32_wrapper::instances, 1u);
  meta_i32_wrapper.destroy(&storage);
  check_eq(i32_wrapper::instances, 0u);
}

TEST("meta objects allow serialization of objects") {
  byte_buffer buf;
  auto meta_i32_wrapper = make_meta_object<i32_wrapper>("i32_wrapper");
  alignas(i32_wrapper) std::byte storage[sizeof(i32_wrapper)];
  binary_serializer sink{nullptr, buf};
  meta_i32_wrapper.default_construct(&storage);
  check_eq(i32_wrapper::instances, 1u);
  check(meta_i32_wrapper.save_binary(sink, &storage));
  i32_wrapper copy;
  check_eq(i32_wrapper::instances, 2u);
  copy.value = 42;
  binary_deserializer source{nullptr, buf};
  check(meta_i32_wrapper.load_binary(source, &copy));
  check_eq(copy.value, 0);
  meta_i32_wrapper.destroy(&storage);
  check_eq(i32_wrapper::instances, 1u);
}

TEST("init_global_meta_objects takes care of creating a meta object table") {
  auto xs = global_meta_objects();
  require_eq(xs.size(), caf::id_block::meta_object_test::end);
  check_eq(type_name_by_id_v<type_id_v<i32_wrapper>>, "i32_wrapper"s);
  check_eq(type_name_by_id_v<type_id_v<i64_wrapper>>, "i64_wrapper"s);
  check_eq(xs[type_id_v<i32_wrapper>].type_name, "i32_wrapper"s);
  check_eq(xs[type_id_v<i64_wrapper>].type_name, "i64_wrapper"s);
  print_debug("calling init_global_meta_objects again is a no-op");
  init_global_meta_objects<id_block::meta_object_test>();
  auto ys = global_meta_objects();
  auto same = [](const auto& x, const auto& y) {
    return x.type_name == y.type_name;
  };
  check(std::equal(xs.begin(), xs.end(), ys.begin(), ys.end(), same));
}

CAF_TEST_MAIN(caf::id_block::meta_object_test)
