// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/typed_actor.hpp"

#include "caf/test/test.hpp"

#include "caf/init_global_meta_objects.hpp"
#include "caf/type_id.hpp"
#include "caf/typed_actor_pointer.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

// --- check invariants of type system -----------------------------------------

// Single signature - trait based and direct template.
struct single_sig_trait {
  using signatures = type_list<result<int>()>;
};
using single_hdl1 = typed_actor<single_sig_trait>;
using single_hdl2 = typed_actor<result<int>()>;

// Dual signature - trait based and direct template.
struct dual_sig_trait {
  using signatures = type_list<result<int>(), result<float>()>;
};
using dual_hdl1 = typed_actor<dual_sig_trait>;
using dual_hdl2 = typed_actor<result<int>(), result<float>()>;

// Check signatures.
static_assert(
  std::is_same_v<single_hdl1::signatures, type_list<result<int>()>>);
static_assert(
  std::is_same_v<single_hdl2::signatures, type_list<result<int>()>>);
static_assert(std::is_same_v<dual_hdl1::signatures,
                             type_list<result<int>(), result<float>()>>);
static_assert(std::is_same_v<dual_hdl2::signatures,
                             type_list<result<int>(), result<float>()>>);

// Check behavior type.
static_assert(
  std::is_same_v<single_hdl1::behavior_type, typed_behavior<single_sig_trait>>);
static_assert(
  std::is_same_v<single_hdl2::behavior_type, typed_behavior<result<int>()>>);
static_assert(
  std::is_same_v<dual_hdl1::behavior_type, typed_behavior<dual_sig_trait>>);
static_assert(std::is_same_v<dual_hdl2::behavior_type,
                             typed_behavior<result<int>(), result<float>()>>);

// Check impl type.
static_assert(
  std::is_same_v<single_hdl1::impl, typed_event_based_actor<single_sig_trait>>);
static_assert(
  std::is_same_v<single_hdl2::impl, typed_event_based_actor<result<int>()>>);
static_assert(
  std::is_same_v<dual_hdl1::impl, typed_event_based_actor<dual_sig_trait>>);
static_assert(
  std::is_same_v<dual_hdl2::impl,
                 typed_event_based_actor<result<int>(), result<float>()>>);

// Check pointer_view type.
static_assert(std::is_same_v<single_hdl1::pointer_view,
                             typed_actor_pointer<single_sig_trait>>);
static_assert(std::is_same_v<single_hdl2::pointer_view,
                             typed_actor_pointer<result<int>()>>);
static_assert(
  std::is_same_v<dual_hdl1::pointer_view, typed_actor_pointer<dual_sig_trait>>);
static_assert(
  std::is_same_v<dual_hdl2::pointer_view,
                 typed_actor_pointer<result<int>(), result<float>()>>);

// Triple signature - trait based and direct template.
struct triple_sig_trait {
  using signatures = type_list<result<int>(), result<float>(), result<bool>()>;
};
using triple_hdl1 = typed_actor<triple_sig_trait>;
using triple_hdl2 = typed_actor<result<int>(), result<float>(), result<bool>()>;

// Check type conversions between trait and directly parametrized actor handles.
static_assert(std::is_constructible_v<single_hdl1, single_hdl2>);
static_assert(std::is_constructible_v<single_hdl2, single_hdl1>);
static_assert(std::is_constructible_v<dual_hdl1, dual_hdl2>);
static_assert(std::is_constructible_v<dual_hdl2, dual_hdl1>);
static_assert(std::is_constructible_v<triple_hdl1, triple_hdl2>);
static_assert(std::is_constructible_v<triple_hdl2, triple_hdl1>);

static_assert(std::is_assignable_v<single_hdl2, single_hdl1>);
static_assert(std::is_assignable_v<single_hdl1, single_hdl2>);
static_assert(std::is_assignable_v<dual_hdl2, dual_hdl1>);
static_assert(std::is_assignable_v<dual_hdl1, dual_hdl2>);
static_assert(std::is_assignable_v<triple_hdl2, triple_hdl1>);
static_assert(std::is_assignable_v<triple_hdl1, triple_hdl2>);

// Check type conversions to handles with a narrower definition.

static_assert(std::is_constructible_v<single_hdl1, dual_hdl1>);
static_assert(std::is_constructible_v<single_hdl2, dual_hdl1>);
static_assert(std::is_constructible_v<single_hdl1, dual_hdl2>);
static_assert(std::is_constructible_v<single_hdl2, dual_hdl2>);
static_assert(std::is_constructible_v<single_hdl1, triple_hdl1>);
static_assert(std::is_constructible_v<single_hdl2, triple_hdl1>);
static_assert(std::is_constructible_v<single_hdl1, triple_hdl2>);
static_assert(std::is_constructible_v<single_hdl2, triple_hdl2>);
static_assert(std::is_constructible_v<dual_hdl1, triple_hdl1>);
static_assert(std::is_constructible_v<dual_hdl2, triple_hdl1>);
static_assert(std::is_constructible_v<dual_hdl1, triple_hdl2>);
static_assert(std::is_constructible_v<dual_hdl2, triple_hdl2>);

static_assert(!std::is_constructible_v<dual_hdl1, single_hdl1>);
static_assert(!std::is_constructible_v<dual_hdl1, single_hdl2>);
static_assert(!std::is_constructible_v<dual_hdl2, single_hdl1>);
static_assert(!std::is_constructible_v<dual_hdl2, single_hdl2>);
static_assert(!std::is_constructible_v<triple_hdl1, single_hdl1>);
static_assert(!std::is_constructible_v<triple_hdl1, single_hdl2>);
static_assert(!std::is_constructible_v<triple_hdl1, dual_hdl1>);
static_assert(!std::is_constructible_v<triple_hdl1, dual_hdl2>);
static_assert(!std::is_constructible_v<triple_hdl2, single_hdl1>);
static_assert(!std::is_constructible_v<triple_hdl2, single_hdl2>);
static_assert(!std::is_constructible_v<triple_hdl2, dual_hdl1>);
static_assert(!std::is_constructible_v<triple_hdl2, dual_hdl2>);

static_assert(std::is_assignable_v<single_hdl1, dual_hdl1>);
static_assert(std::is_assignable_v<single_hdl2, dual_hdl1>);
static_assert(std::is_assignable_v<single_hdl1, dual_hdl2>);
static_assert(std::is_assignable_v<single_hdl2, dual_hdl2>);
static_assert(std::is_assignable_v<single_hdl1, triple_hdl1>);
static_assert(std::is_assignable_v<single_hdl2, triple_hdl1>);
static_assert(std::is_assignable_v<single_hdl1, triple_hdl2>);
static_assert(std::is_assignable_v<single_hdl2, triple_hdl2>);
static_assert(std::is_assignable_v<dual_hdl1, triple_hdl1>);
static_assert(std::is_assignable_v<dual_hdl2, triple_hdl1>);
static_assert(std::is_assignable_v<dual_hdl1, triple_hdl2>);
static_assert(std::is_assignable_v<dual_hdl2, triple_hdl2>);

static_assert(!std::is_assignable_v<dual_hdl1, single_hdl1>);
static_assert(!std::is_assignable_v<dual_hdl1, single_hdl2>);
static_assert(!std::is_assignable_v<dual_hdl2, single_hdl1>);
static_assert(!std::is_assignable_v<dual_hdl2, single_hdl2>);
static_assert(!std::is_assignable_v<triple_hdl1, single_hdl1>);
static_assert(!std::is_assignable_v<triple_hdl1, single_hdl2>);
static_assert(!std::is_assignable_v<triple_hdl1, dual_hdl1>);
static_assert(!std::is_assignable_v<triple_hdl1, dual_hdl2>);
static_assert(!std::is_assignable_v<triple_hdl2, single_hdl1>);
static_assert(!std::is_assignable_v<triple_hdl2, single_hdl2>);
static_assert(!std::is_assignable_v<triple_hdl2, dual_hdl1>);
static_assert(!std::is_assignable_v<triple_hdl2, dual_hdl2>);

// --- check extension API of type system --------------------------------------

struct my_trait1 {
  using signatures = type_list<result<void>(int, int), result<double>(double)>;
};
using dummy1 = typed_actor<my_trait1>;
using dummy2_sig = dummy1::extend<result<void>(ok_atom)>;
static_assert(
  std::is_same_v<dummy2_sig::signatures,
                 type_list<result<void>(int, int), result<double>(double),
                           result<void>(ok_atom)>>);
static_assert(std::is_convertible_v<dummy2_sig, dummy1>,
              "handle not assignable to narrower definition");
struct my_trait2 {
  using signatures = type_list<result<void>(ok_atom)>;
};
using dummy2_trait = dummy1::extend_with<my_trait2>;
static_assert(
  std::is_same_v<dummy2_trait::signatures,
                 type_list<result<void>(int, int), result<double>(double),
                           result<void>(ok_atom)>>);
static_assert(std::is_convertible_v<dummy2_trait, dummy1>,
              "handle not assignable to narrower definition");
struct my_trait3 {
  using signatures = type_list<result<void>(float, int)>;
};
using dummy3 = typed_actor<my_trait3>;
struct my_trait4 {
  using signatures = type_list<result<double>(int)>;
};
using dummy4 = typed_actor<my_trait4>;
using dummy5 = dummy4::extend_with<dummy3>;
static_assert(
  std::is_same_v<dummy5::signatures,
                 type_list<result<double>(int), result<void>(float, int)>>);
static_assert(std::is_convertible_v<dummy5, dummy3>,
              "handle not assignable to narrower definition");
static_assert(std::is_convertible_v<dummy5, dummy4>,
              "handle not assignable to narrower definition");

static_assert(
  std::is_same_v<type_list<bool>::append_from<type_list<int>, type_list<float>>,
                 type_list<bool, int, float>>);

struct s01 {};
struct s02 {};
struct s03 {};
struct s04 {};
struct s05 {};
struct s06 {};
struct s07 {};
struct s08 {};
struct s09 {};
struct s10 {};
struct s11 {};
struct s12 {};
struct s13 {};
struct s14 {};
struct s15 {};
struct s16 {};
struct s17 {};
struct s18 {};
struct s19 {};
struct s20 {};

struct t1970 {
  using signatures = type_list<
    result<void>(s01), result<void>(s15), result<void>(s07), result<void>(s12),
    result<void>(s19), result<void>(s03), result<void>(s10), result<void>(s20),
    result<void>(s05), result<void>(s14), result<void>(s08), result<void>(s17),
    result<void>(s02), result<void>(s18), result<void>(s11), result<void>(s04),
    result<void>(s13), result<void>(s09), result<void>(s16), result<void>(s06)>;
};

using a1970 = typed_actor<t1970>;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(typed_actor_test, caf::first_custom_type_id + 130)

  CAF_ADD_TYPE_ID(typed_actor_test, (s01))
  CAF_ADD_TYPE_ID(typed_actor_test, (s02))
  CAF_ADD_TYPE_ID(typed_actor_test, (s03))
  CAF_ADD_TYPE_ID(typed_actor_test, (s04))
  CAF_ADD_TYPE_ID(typed_actor_test, (s05))
  CAF_ADD_TYPE_ID(typed_actor_test, (s06))
  CAF_ADD_TYPE_ID(typed_actor_test, (s07))
  CAF_ADD_TYPE_ID(typed_actor_test, (s08))
  CAF_ADD_TYPE_ID(typed_actor_test, (s09))
  CAF_ADD_TYPE_ID(typed_actor_test, (s10))
  CAF_ADD_TYPE_ID(typed_actor_test, (s11))
  CAF_ADD_TYPE_ID(typed_actor_test, (s12))
  CAF_ADD_TYPE_ID(typed_actor_test, (s13))
  CAF_ADD_TYPE_ID(typed_actor_test, (s14))
  CAF_ADD_TYPE_ID(typed_actor_test, (s15))
  CAF_ADD_TYPE_ID(typed_actor_test, (s16))
  CAF_ADD_TYPE_ID(typed_actor_test, (s17))
  CAF_ADD_TYPE_ID(typed_actor_test, (s18))
  CAF_ADD_TYPE_ID(typed_actor_test, (s19))
  CAF_ADD_TYPE_ID(typed_actor_test, (s20))

CAF_END_TYPE_ID_BLOCK(typed_actor_test)

TEST("GH-1970 regression") {
  // The reported issue was that typed actors would blow up to several dozens of
  // GB of memory usage during compile time when instantiating a typed actor
  // with a large number of message handlers, e.g., 20. So the regression test
  // simply checks that the following code actually compiles, as opposed to
  // OOMing the compiler.
  auto bhvr = typed_behavior<t1970>{
    [](s01) {}, [](s02) {}, [](s03) {}, [](s04) {}, [](s05) {},
    [](s06) {}, [](s07) {}, [](s08) {}, [](s09) {}, [](s10) {},
    [](s11) {}, [](s12) {}, [](s13) {}, [](s14) {}, [](s15) {},
    [](s16) {}, [](s17) {}, [](s18) {}, [](s19) {}, [](s20) {},
  };
}

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::typed_actor_test>();
}
