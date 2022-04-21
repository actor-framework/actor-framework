// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE metaprogramming

#include "core-test.hpp"

#include <cstdint>
#include <string>
#include <type_traits>
#include <typeinfo>

#include "caf/all.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"

using namespace caf;
using namespace caf::detail;

namespace {

// misc

template <class T>
struct is_int : std::false_type {};

template <>
struct is_int<int> : std::true_type {};

} // namespace

CAF_TEST(metaprogramming) {
  using std::is_same;
  using l1 = type_list<int, float, std::string>;
  using r1 = tl_reverse<l1>::type;
  CHECK((is_same<int, tl_at<l1, 0>::type>::value));
  CHECK((is_same<float, tl_at<l1, 1>::type>::value));
  CHECK((is_same<std::string, tl_at<l1, 2>::type>::value));
  CHECK_EQ(3u, tl_size<l1>::value);
  CHECK_EQ(tl_size<r1>::value, tl_size<l1>::value);
  CHECK((is_same<tl_at<l1, 0>::type, tl_at<r1, 2>::type>::value));
  CHECK((is_same<tl_at<l1, 1>::type, tl_at<r1, 1>::type>::value));
  CHECK((is_same<tl_at<l1, 2>::type, tl_at<r1, 0>::type>::value));
  using l2 = tl_concat<type_list<int>, l1>::type;
  CHECK((is_same<int, tl_head<l2>::type>::value));
  CHECK((is_same<l1, tl_tail<l2>::type>::value));
  CHECK_EQ((detail::tl_count<l1, is_int>::value), 1u);
  CHECK_EQ((detail::tl_count<l2, is_int>::value), 2u);
  using il0 = int_list<0, 1, 2, 3, 4, 5>;
  using il1 = int_list<4, 5>;
  using il2 = il_right<il0, 2>::type;
  CHECK((is_same<il2, il1>::value));
  /* test tl_subset_of */ {
    using list_a = type_list<int, float, double>;
    using list_b = type_list<float, int, double, std::string>;
    CHECK((tl_subset_of<list_a, list_b>::value));
    CHECK(!(tl_subset_of<list_b, list_a>::value));
    CHECK((tl_subset_of<list_a, list_a>::value));
    CHECK((tl_subset_of<list_b, list_b>::value));
  }
}

template <class T>
struct token {};

template <class T>
std::ostream& operator<<(std::ostream& out, token<T>) {
  return out << typeid(T).name();
}

template <class T, class U>
constexpr bool operator==(token<T>, token<U>) {
  return std::is_same<T, U>::value;
}

template <class T>
constexpr token<T> tk() {
  return {};
}

template <class T, class U>
constexpr token<response_type_unbox_t<T, U>> res(token<T>, token<U>) {
  return {};
}

template <class T, class U>
constexpr token<none_t> res(T, U) {
  return {};
}

template <class T, class U>
constexpr token<composed_type_t<T, U>> dot_op(token<T>, token<U>) {
  return {};
}

// -- lift a list of callback types into a list of MPIs

// -- typed behavior dummy class

template <class... Ifs>
struct typed_beh {
  template <class... Ts>
  typed_beh(Ts&&... xs) {
    assign(std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  typename std::enable_if<sizeof...(Ifs) == sizeof...(Ts)>::type assign(Ts...) {
    using expected = type_list<Ifs...>;
    using found = type_list<deduce_mpi_t<Ts>...>;
    pos = interface_mismatch_t<found, expected>::value;
    valid = pos == sizeof...(Ifs);
  }

  template <class... Ts>
  typename std::enable_if<sizeof...(Ifs) != sizeof...(Ts)>::type assign(Ts...) {
    // too many or too few handlers present
    pos = -1;
    valid = false;
  }

  bool valid = false;
  int pos = 0;
};

using bi_pair = std::pair<bool, int>;

template <class TypedBehavior, class... Ts>
bi_pair tb_assign(Ts&&... xs) {
  TypedBehavior x{std::forward<Ts>(xs)...};
  return {x.valid, x.pos};
}

namespace std {

ostream& operator<<(ostream& out, const pair<bool, int>& x) {
  // do not modify stream with boolalpha
  return out << '(' << (x.first ? "true" : "false") << ", " << x.second << ')';
}

} // namespace std

CAF_TEST(typed_behavior_assignment) {
  using bh1 = typed_beh<result<double>(int), result<int, int>(double, double)>;
  // compatible handlers resulting in perfect match
  auto f1 = [=](int) { return 0.; };
  auto f2 = [=](double, double) -> result<int, int> { return {0, 0}; };
  // incompatbile handlers
  auto e1 = [=](int) { return 0.f; };
  auto e2 = [=](double, double) { return std::make_tuple(0.f, 0.f); };
  // omit one handler
  CHECK_EQ(bi_pair(false, -1), tb_assign<bh1>(f1));
  CHECK_EQ(bi_pair(false, -1), tb_assign<bh1>(f2));
  CHECK_EQ(bi_pair(false, -1), tb_assign<bh1>(e1));
  CHECK_EQ(bi_pair(false, -1), tb_assign<bh1>(e2));
  // any valid alteration of (f1, f2)
  CHECK_EQ(bi_pair(true, 2), tb_assign<bh1>(f1, f2));
  CHECK_EQ(bi_pair(true, 2), tb_assign<bh1>(f2, f1));
  // any invalid alteration of (f1, f2, e1, e2)
  CHECK_EQ(bi_pair(false, 1), tb_assign<bh1>(f1, e1));
  CHECK_EQ(bi_pair(false, 1), tb_assign<bh1>(f1, e2));
  CHECK_EQ(bi_pair(false, 0), tb_assign<bh1>(e1, f1));
  CHECK_EQ(bi_pair(false, 0), tb_assign<bh1>(e1, f2));
  CHECK_EQ(bi_pair(false, 0), tb_assign<bh1>(e1, e2));
  CHECK_EQ(bi_pair(false, 1), tb_assign<bh1>(f2, e1));
  CHECK_EQ(bi_pair(false, 1), tb_assign<bh1>(f2, e2));
  CHECK_EQ(bi_pair(false, 0), tb_assign<bh1>(e2, f1));
  CHECK_EQ(bi_pair(false, 0), tb_assign<bh1>(e2, f2));
  CHECK_EQ(bi_pair(false, 0), tb_assign<bh1>(e2, e1));
  using bh2
    = typed_beh<result<void>(int),           //
                result<void>(int, int),      //
                result<void>(int, int, int), //
                result<void>(int, int, int, int),
                result<void>(int, int, int, int, int),
                result<void>(int, int, int, int, int, int),
                result<void>(int, int, int, int, int, int, int),
                result<void>(int, int, int, int, int, int, int, int),
                result<void>(int, int, int, int, int, int, int, int, int),
                result<void>(int, int, int, int, int, int, int, int, int, int)>;
  auto h0 = [](int) {};
  auto h1 = [](int, int) {};
  auto h2 = [](int, int, int) {};
  auto h3 = [](int, int, int, int) {};
  auto h4 = [](int, int, int, int, int) {};
  auto h5 = [](int, int, int, int, int, int) {};
  auto h6 = [](int, int, int, int, int, int, int) {};
  auto h7 = [](int, int, int, int, int, int, int, int) {};
  auto h8 = [](int, int, int, int, int, int, int, int, int) {};
  auto h9 = [](int, int, int, int, int, int, int, int, int, int) {};
  CHECK_EQ(bi_pair(true, 10),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9));
  CHECK_EQ(bi_pair(false, 0),
           tb_assign<bh2>(e1, h1, h2, h3, h4, h5, h6, h7, h8, h9));
  CHECK_EQ(bi_pair(false, 1),
           tb_assign<bh2>(h0, e1, h2, h3, h4, h5, h6, h7, h8, h9));
  CHECK_EQ(bi_pair(false, 2),
           tb_assign<bh2>(h0, h1, e1, h3, h4, h5, h6, h7, h8, h9));
  CHECK_EQ(bi_pair(false, 3),
           tb_assign<bh2>(h0, h1, h2, e1, h4, h5, h6, h7, h8, h9));
  CHECK_EQ(bi_pair(false, 4),
           tb_assign<bh2>(h0, h1, h2, h3, e1, h5, h6, h7, h8, h9));
  CHECK_EQ(bi_pair(false, 5),
           tb_assign<bh2>(h0, h1, h2, h3, h4, e1, h6, h7, h8, h9));
  CHECK_EQ(bi_pair(false, 6),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, e1, h7, h8, h9));
  CHECK_EQ(bi_pair(false, 7),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, h6, e1, h8, h9));
  CHECK_EQ(bi_pair(false, 8),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, h6, h7, e1, h9));
  CHECK_EQ(bi_pair(false, 9),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, h6, h7, h8, e1));
  CHECK_EQ(bi_pair(false, -1),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, h6, h7, h8));
}

struct foo {};
struct bar {};
bool operator==(const bar&, const bar&);
class baz {
public:
  baz() = default;

  explicit baz(std::string);

  friend bool operator==(const baz&, const baz&);

private:
  std::string str_;
};

CAF_TEST(is_comparable) {
  CHECK((is_comparable<double, std::string>::value) == false);
  CHECK((is_comparable<foo, foo>::value) == false);
  CHECK((is_comparable<bar, bar>::value) == true);
  CHECK((is_comparable<double, bar>::value) == false);
  CHECK((is_comparable<bar, double>::value) == false);
  CHECK((is_comparable<baz, baz>::value) == true);
  CHECK((is_comparable<double, baz>::value) == false);
  CHECK((is_comparable<baz, double>::value) == false);
  CHECK((is_comparable<std::string, baz>::value) == false);
  CHECK((is_comparable<baz, std::string>::value) == false);
}
