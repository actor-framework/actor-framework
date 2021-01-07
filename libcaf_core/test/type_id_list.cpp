// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE type_id_list

#include "caf/type_id_list.hpp"

#include "core-test.hpp"

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

CAF_BEGIN_TYPE_ID_BLOCK(type_id_test, caf::id_block::core_test::end)

  CAF_ADD_TYPE_ID(type_id_test, (detail::my_secret))
  CAF_ADD_TYPE_ID(type_id_test, (io::protocol))

CAF_END_TYPE_ID_BLOCK(type_id_test)

using namespace caf;

CAF_TEST(lists store the size at index 0) {
  type_id_t data[] = {3, 1, 2, 4};
  type_id_list xs{data};
  CAF_CHECK_EQUAL(xs.size(), 3u);
  CAF_CHECK_EQUAL(xs[0], 1u);
  CAF_CHECK_EQUAL(xs[1], 2u);
  CAF_CHECK_EQUAL(xs[2], 4u);
}

CAF_TEST(lists are comparable) {
  type_id_t data[] = {3, 1, 2, 4};
  type_id_list xs{data};
  type_id_t data_copy[] = {3, 1, 2, 4};
  type_id_list ys{data_copy};
  CAF_CHECK_EQUAL(xs, ys);
  data_copy[1] = 10;
  CAF_CHECK_NOT_EQUAL(xs, ys);
  CAF_CHECK_LESS(xs, ys);
  CAF_CHECK_EQUAL(make_type_id_list<add_atom>(), make_type_id_list<add_atom>());
  CAF_CHECK_NOT_EQUAL(make_type_id_list<add_atom>(),
                      make_type_id_list<ok_atom>());
}

CAF_TEST(make_type_id_list constructs a list from types) {
  auto xs = make_type_id_list<uint8_t, bool, float>();
  CAF_CHECK_EQUAL(xs.size(), 3u);
  CAF_CHECK_EQUAL(xs[0], type_id_v<uint8_t>);
  CAF_CHECK_EQUAL(xs[1], type_id_v<bool>);
  CAF_CHECK_EQUAL(xs[2], type_id_v<float>);
}

CAF_TEST(type ID lists are convertible to strings) {
  auto xs = make_type_id_list<uint16_t, bool, float, long double>();
  CAF_CHECK_EQUAL(to_string(xs), "[uint16_t, bool, float, ldouble]");
}
