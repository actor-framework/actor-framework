// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/meta/handler.hpp"

#include "caf/test/test.hpp"

#include "caf/type_id.hpp"

using namespace caf;

TEST("handlers are convertible to strings") {
  check_eq(to_string(meta::handler{
             make_type_id_list<>(),
             make_type_id_list<>(),
           }),
           "() -> ()");
  check_eq(to_string(meta::handler{
             make_type_id_list<int32_t>(),
             make_type_id_list<>(),
           }),
           "(int32_t) -> ()");
  check_eq(to_string(meta::handler{
             make_type_id_list<>(),
             make_type_id_list<int32_t>(),
           }),
           "() -> (int32_t)");
  check_eq(to_string(meta::handler{
             make_type_id_list<int32_t, int16_t, int8_t>(),
             make_type_id_list<int8_t, int16_t, int32_t>(),
           }),
           "(int32_t, int16_t, int8_t) -> (int8_t, int16_t, int32_t)");
}

TEST("handlers are comparable") {
  check_eq(
    meta::handler{
      make_type_id_list<int32_t>(),
      make_type_id_list<>(),
    },
    meta::handler{
      make_type_id_list<int32_t>(),
      make_type_id_list<>(),
    });
  check_ne(
    meta::handler{
      make_type_id_list<int32_t>(),
      make_type_id_list<>(),
    },
    meta::handler{
      make_type_id_list<int16_t>(),
      make_type_id_list<>(),
    });
  check_ne(
    meta::handler{
      make_type_id_list<int32_t>(),
      make_type_id_list<>(),
    },
    meta::handler{
      make_type_id_list<int32_t>(),
      make_type_id_list<int8_t>(),
    });
}

TEST("handler lists can check for assignability of statically typed actors") {
  // Simple get/put interface for integers.
  using if1
    = type_list<result<void>(put_atom, int32_t), result<int32_t>(get_atom)>;
  // Same as if1, but with reversed order of arguments.
  using if2
    = type_list<result<int32_t>(get_atom), result<void>(put_atom, int32_t)>;
  // Extends if1 with a string argument.
  using if3 = if1::append<result<void>(put_atom, std::string),
                          result<std::string>(get_atom)>;
  // Unrelated to all interfaces above.
  using if4 = type_list<result<void>(ok_atom)>;
  auto ls1 = meta::handlers_from_signature_list<if1>::handlers;
  auto ls2 = meta::handlers_from_signature_list<if2>::handlers;
  auto ls3 = meta::handlers_from_signature_list<if3>::handlers;
  auto ls4 = meta::handlers_from_signature_list<if4>::handlers;
  SECTION("each interface is assignable to itself") {
    check(meta::assignable(ls1, ls1));
    check(meta::assignable(ls2, ls2));
    check(meta::assignable(ls3, ls3));
    check(meta::assignable(ls4, ls4));
  }
  SECTION("ls1 and ls2 are assignable to each other") {
    check(meta::assignable(ls1, ls2));
    check(meta::assignable(ls2, ls1));
  }
  SECTION("ls1 can assign from ls3 but not vice versa") {
    check(meta::assignable(ls1, ls3));
    check(!meta::assignable(ls3, ls1));
  }
  SECTION("ls2 can assign from ls3 but not vice versa") {
    check(meta::assignable(ls2, ls3));
    check(!meta::assignable(ls3, ls2));
  }
  SECTION("ls4 is incompatible with all other interfaces") {
    check(!meta::assignable(ls1, ls4));
    check(!meta::assignable(ls2, ls4));
    check(!meta::assignable(ls3, ls4));
    check(!meta::assignable(ls4, ls1));
    check(!meta::assignable(ls4, ls2));
    check(!meta::assignable(ls4, ls3));
  }
  SECTION("the empty interface can be assigned to from all others") {
    // Empty interface.
    using if5 = type_list<>;
    auto ls5 = meta::handlers_from_signature_list<if5>::handlers;
    check(meta::assignable(ls5, ls5));
    check(meta::assignable(ls5, ls1));
    check(meta::assignable(ls5, ls2));
    check(meta::assignable(ls5, ls3));
    check(meta::assignable(ls5, ls4));
    check(!meta::assignable(ls1, ls5));
    check(!meta::assignable(ls2, ls5));
    check(!meta::assignable(ls3, ls5));
    check(!meta::assignable(ls4, ls5));
  }
}
