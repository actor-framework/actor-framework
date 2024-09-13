// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/type_id_list.hpp"

#include "caf/test/test.hpp"

#include "caf/init_global_meta_objects.hpp"

namespace detail {

struct my_secret {
  int value;
};

template <class Inspector>
bool inspect(Inspector& f, my_secret& x) {
  return f.object(x).fields(f.field("value", x.value));
}

} // namespace detail

namespace io {

struct protocol {
  std::string name;
};

template <class Inspector>
bool inspect(Inspector& f, protocol& x) {
  return f.object(x).fields(f.field("name", x.name));
}

} // namespace io

// A type ID block with types that live in namespaces that also exist as nested
// CAF namespaces. This is a regression test for
// https://github.com/actor-framework/actor-framework/issues/1195. We don't need
// to actually use these types, only check whether the code compiles.

CAF_BEGIN_TYPE_ID_BLOCK(type_id_test, caf::first_custom_type_id + 20)

  CAF_ADD_TYPE_ID(type_id_test, (detail::my_secret))
  CAF_ADD_TYPE_ID(type_id_test, (io::protocol))

CAF_END_TYPE_ID_BLOCK(type_id_test)

using namespace caf;

TEST("lists store the size at index 0") {
  type_id_t data[] = {3, 1, 2, 4};
  type_id_list xs{data};
  check_eq(xs.size(), 3u);
  check_eq(xs[0], 1u);
  check_eq(xs[1], 2u);
  check_eq(xs[2], 4u);
}

TEST("lists are comparable") {
  type_id_t data[] = {3, 1, 2, 4};
  type_id_list xs{data};
  type_id_t data_copy[] = {3, 1, 2, 4};
  type_id_list ys{data_copy};
  check_eq(xs, ys);
  data_copy[1] = 10;
  check_ne(xs, ys);
  check_lt(xs, ys);
  check_eq(make_type_id_list<add_atom>(), make_type_id_list<add_atom>());
  check_ne(make_type_id_list<add_atom>(), make_type_id_list<ok_atom>());
}

TEST("make_type_id_list constructs a list from types") {
  auto xs = make_type_id_list<uint8_t, bool, float>();
  check_eq(xs.size(), 3u);
  check_eq(xs[0], type_id_v<uint8_t>);
  check_eq(xs[1], type_id_v<bool>);
  check_eq(xs[2], type_id_v<float>);
}

TEST("type ID lists are convertible to strings") {
  auto xs = make_type_id_list<uint16_t, bool, float, long double>();
  check_eq(to_string(xs), "[uint16_t, bool, float, ldouble]");
}

TEST("type ID lists are concatenable") {
  // 1 + 0
  check_eq((make_type_id_list<int8_t>()),
           type_id_list::concat(make_type_id_list<int8_t>(),
                                make_type_id_list<>()));
  check_eq((make_type_id_list<int8_t>()),
           type_id_list::concat(make_type_id_list<>(),
                                make_type_id_list<int8_t>()));
  // 1 + 1
  check_eq((make_type_id_list<int8_t, int16_t>()),
           type_id_list::concat(make_type_id_list<int8_t>(),
                                make_type_id_list<int16_t>()));
  // 2 + 0
  check_eq((make_type_id_list<int8_t, int16_t>()),
           type_id_list::concat(make_type_id_list<int8_t, int16_t>(),
                                make_type_id_list<>()));
  check_eq((make_type_id_list<int8_t, int16_t>()),
           type_id_list::concat(make_type_id_list<>(),
                                make_type_id_list<int8_t, int16_t>()));
  // 2 + 1
  check_eq((make_type_id_list<int8_t, int16_t, int32_t>()),
           type_id_list::concat(make_type_id_list<int8_t, int16_t>(),
                                make_type_id_list<int32_t>()));
  check_eq((make_type_id_list<int8_t, int16_t, int32_t>()),
           type_id_list::concat(make_type_id_list<int8_t>(),
                                make_type_id_list<int16_t, int32_t>()));
  // 2 + 2
  check_eq((make_type_id_list<int8_t, int16_t, int32_t, int64_t>()),
           type_id_list::concat(make_type_id_list<int8_t, int16_t>(),
                                make_type_id_list<int32_t, int64_t>()));
}
