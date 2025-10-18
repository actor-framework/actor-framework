// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/type_list.hpp"

#include "caf/test/test.hpp"

using std::is_same_v;

namespace caf::detail {

namespace {

struct sx {};
struct s1 {};
struct s2 {};
struct s3 {};
struct s4 {};

template <class R, class... Ts>
using rm_t = tl_remove_t<type_list<Ts...>, R>;

template <class... Ts>
using tl_t = type_list<Ts...>;

} // namespace

TEST("tl_remove_t removes elements from a type list") {
  // Note: each section has a dummy check since all actual checks are done at
  //       compile-time.
  SECTION("removing a type from an empty list") {
    static_assert(is_same_v<rm_t<s1>, tl_t<>>);
    check(true);
  }
  SECTION("removing a type from a single element list") {
    static_assert(is_same_v<rm_t<sx, sx>, tl_t<>>);
    static_assert(is_same_v<rm_t<sx, s1>, tl_t<s1>>);
    check(true);
  }
  SECTION("removing a type from a two element list") {
    static_assert(is_same_v<rm_t<sx, s1, s2>, tl_t<s1, s2>>);
    static_assert(is_same_v<rm_t<sx, s1, sx>, tl_t<s1>>);
    static_assert(is_same_v<rm_t<sx, sx, s2>, tl_t<s2>>);
    static_assert(is_same_v<rm_t<sx, sx, sx>, tl_t<>>);
    check(true);
  }
  SECTION("removing a type from a three element list") {
    static_assert(is_same_v<rm_t<sx, s1, s2, s3>, tl_t<s1, s2, s3>>);
    static_assert(is_same_v<rm_t<sx, s1, s2, sx>, tl_t<s1, s2>>);
    static_assert(is_same_v<rm_t<sx, s1, sx, s3>, tl_t<s1, s3>>);
    static_assert(is_same_v<rm_t<sx, s1, sx, sx>, tl_t<s1>>);
    static_assert(is_same_v<rm_t<sx, sx, s2, s3>, tl_t<s2, s3>>);
    static_assert(is_same_v<rm_t<sx, sx, s2, sx>, tl_t<s2>>);
    static_assert(is_same_v<rm_t<sx, sx, sx, s3>, tl_t<s3>>);
    static_assert(is_same_v<rm_t<sx, sx, sx, sx>, tl_t<>>);
    check(true);
  }
  SECTION("removing a type from a four element list") {
    static_assert(is_same_v<rm_t<sx, s1, s2, s3, s4>, tl_t<s1, s2, s3, s4>>);
    static_assert(is_same_v<rm_t<sx, s1, s2, s3, sx>, tl_t<s1, s2, s3>>);
    static_assert(is_same_v<rm_t<sx, s1, s2, sx, s4>, tl_t<s1, s2, s4>>);
    static_assert(is_same_v<rm_t<sx, s1, s2, sx, sx>, tl_t<s1, s2>>);
    static_assert(is_same_v<rm_t<sx, s1, sx, s3, s4>, tl_t<s1, s3, s4>>);
    static_assert(is_same_v<rm_t<sx, s1, sx, s3, sx>, tl_t<s1, s3>>);
    static_assert(is_same_v<rm_t<sx, s1, sx, sx, s4>, tl_t<s1, s4>>);
    static_assert(is_same_v<rm_t<sx, s1, sx, sx, sx>, tl_t<s1>>);
    static_assert(is_same_v<rm_t<sx, sx, s2, s3, s4>, tl_t<s2, s3, s4>>);
    static_assert(is_same_v<rm_t<sx, sx, s2, s3, sx>, tl_t<s2, s3>>);
    static_assert(is_same_v<rm_t<sx, sx, s2, sx, s4>, tl_t<s2, s4>>);
    static_assert(is_same_v<rm_t<sx, sx, s2, sx, sx>, tl_t<s2>>);
    static_assert(is_same_v<rm_t<sx, sx, sx, s3, s4>, tl_t<s3, s4>>);
    static_assert(is_same_v<rm_t<sx, sx, sx, s3, sx>, tl_t<s3>>);
    static_assert(is_same_v<rm_t<sx, sx, sx, sx, s4>, tl_t<s4>>);
    static_assert(is_same_v<rm_t<sx, sx, sx, sx, sx>, tl_t<>>);
    check(true);
  }
}

} // namespace caf::detail
