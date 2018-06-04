/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE metaprogramming
#include "caf/test/unit_test.hpp"

#include <string>
#include <cstdint>
#include <typeinfo>
#include <type_traits>

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

} // namespace <anonymous>

CAF_TEST(metaprogramming) {
  using std::is_same;
  using l1 = type_list<int, float, std::string>;
  using r1 = tl_reverse<l1>::type;
  CAF_CHECK((is_same<int, tl_at<l1, 0>::type>::value));
  CAF_CHECK((is_same<float, tl_at<l1, 1>::type>::value));
  CAF_CHECK((is_same<std::string, tl_at<l1, 2>::type>::value));
  CAF_CHECK_EQUAL(3u, tl_size<l1>::value);
  CAF_CHECK_EQUAL(tl_size<r1>::value, tl_size<l1>::value);
  CAF_CHECK((is_same<tl_at<l1, 0>::type, tl_at<r1, 2>::type>::value));
  CAF_CHECK((is_same<tl_at<l1, 1>::type, tl_at<r1, 1>::type>::value));
  CAF_CHECK((is_same<tl_at<l1, 2>::type, tl_at<r1, 0>::type>::value));
  using l2 = tl_concat<type_list<int>, l1>::type;
  CAF_CHECK((is_same<int, tl_head<l2>::type>::value));
  CAF_CHECK((is_same<l1, tl_tail<l2>::type>::value));
  CAF_CHECK_EQUAL((detail::tl_count<l1, is_int>::value), 1u);
  CAF_CHECK_EQUAL((detail::tl_count<l2, is_int>::value), 2u);
  using il0 = int_list<0, 1, 2, 3, 4, 5>;
  using il1 = int_list<4, 5>;
  using il2 = il_right<il0, 2>::type;
  CAF_CHECK((is_same<il2, il1>::value));
  /* test tl_subset_of */ {
    using list_a = type_list<int, float, double>;
    using list_b = type_list<float, int, double, std::string>;
    CAF_CHECK((tl_subset_of<list_a, list_b>::value));
    CAF_CHECK(!(tl_subset_of<list_b, list_a>::value));
    CAF_CHECK((tl_subset_of<list_a, list_a>::value));
    CAF_CHECK((tl_subset_of<list_b, list_b>::value));
  }
}

template <class T>
struct token { };

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
  typename std::enable_if<sizeof...(Ifs) == sizeof...(Ts)>::type
  assign(Ts...) {
    using expected = type_list<Ifs...>;
    using found = type_list<deduce_mpi_t<Ts>...>;
    pos = interface_mismatch_t<found, expected>::value;
    valid = pos == sizeof...(Ifs);
  }

  template <class... Ts>
  typename std::enable_if<sizeof...(Ifs) != sizeof...(Ts)>::type
  assign(Ts...) {
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
  using bh1 = typed_beh<replies_to<int>::with<double>,
                        replies_to<double, double>::with<int, int>>;
  // compatible handlers resulting in perfect match
  auto f1 = [=](int) { return 0.; };
  auto f2 = [=](double, double) { return std::make_tuple(0, 0); };
  // compatible handlers using skip
  auto g1 = [=](int) { return skip(); };
  auto g2 = [=](double, double) { return skip(); };
  // incompatbile handlers
  auto e1 = [=](int) { return 0.f; };
  auto e2 = [=](double, double) { return std::make_tuple(0.f, 0.f); };
  // omit one handler
  CAF_CHECK_EQUAL(bi_pair(false, -1), tb_assign<bh1>(f1));
  CAF_CHECK_EQUAL(bi_pair(false, -1), tb_assign<bh1>(f2));
  CAF_CHECK_EQUAL(bi_pair(false, -1), tb_assign<bh1>(g1));
  CAF_CHECK_EQUAL(bi_pair(false, -1), tb_assign<bh1>(g2));
  CAF_CHECK_EQUAL(bi_pair(false, -1), tb_assign<bh1>(e1));
  CAF_CHECK_EQUAL(bi_pair(false, -1), tb_assign<bh1>(e2));
  // any valid alteration of (f1, f2, g1, g2)
  CAF_CHECK_EQUAL(bi_pair(true, 2), tb_assign<bh1>(f1, f2));
  CAF_CHECK_EQUAL(bi_pair(true, 2), tb_assign<bh1>(f2, f1));
  CAF_CHECK_EQUAL(bi_pair(true, 2), tb_assign<bh1>(g1, g2));
  CAF_CHECK_EQUAL(bi_pair(true, 2), tb_assign<bh1>(g2, g1));
  CAF_CHECK_EQUAL(bi_pair(true, 2), tb_assign<bh1>(g1, f2));
  CAF_CHECK_EQUAL(bi_pair(true, 2), tb_assign<bh1>(f2, g1));
  CAF_CHECK_EQUAL(bi_pair(true, 2), tb_assign<bh1>(f1, g2));
  CAF_CHECK_EQUAL(bi_pair(true, 2), tb_assign<bh1>(g2, f1));
  // any invalid alteration of (f1, f2, g1, g2)
  CAF_CHECK_EQUAL(bi_pair(false, 1), tb_assign<bh1>(f1, g1));
  CAF_CHECK_EQUAL(bi_pair(false, 1), tb_assign<bh1>(g1, f1));
  CAF_CHECK_EQUAL(bi_pair(false, 1), tb_assign<bh1>(f2, g2));
  CAF_CHECK_EQUAL(bi_pair(false, 1), tb_assign<bh1>(g2, g2));
  // any invalid alteration of (f1, f2, e1, e2)
  CAF_CHECK_EQUAL(bi_pair(false, 1), tb_assign<bh1>(f1, e1));
  CAF_CHECK_EQUAL(bi_pair(false, 1), tb_assign<bh1>(f1, e2));
  CAF_CHECK_EQUAL(bi_pair(false, 0), tb_assign<bh1>(e1, f1));
  CAF_CHECK_EQUAL(bi_pair(false, 0), tb_assign<bh1>(e1, f2));
  CAF_CHECK_EQUAL(bi_pair(false, 0), tb_assign<bh1>(e1, e2));
  CAF_CHECK_EQUAL(bi_pair(false, 1), tb_assign<bh1>(f2, e1));
  CAF_CHECK_EQUAL(bi_pair(false, 1), tb_assign<bh1>(f2, e2));
  CAF_CHECK_EQUAL(bi_pair(false, 0), tb_assign<bh1>(e2, f1));
  CAF_CHECK_EQUAL(bi_pair(false, 0), tb_assign<bh1>(e2, f2));
  CAF_CHECK_EQUAL(bi_pair(false, 0), tb_assign<bh1>(e2, e1));
  using bh2 = typed_beh<reacts_to<int>,
                        reacts_to<int, int>,
                        reacts_to<int, int, int>,
                        reacts_to<int, int, int, int>,
                        reacts_to<int, int, int, int, int>,
                        reacts_to<int, int, int, int, int, int>,
                        reacts_to<int, int, int, int, int, int, int>,
                        reacts_to<int, int, int, int, int, int, int, int>,
                        reacts_to<int, int, int, int, int,
                                     int, int, int, int>,
                        reacts_to<int, int, int, int, int,
                                     int, int, int, int, int>>;
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
  CAF_CHECK_EQUAL(bi_pair(true, 10), tb_assign<bh2>(h0, h1, h2, h3, h4,
                                                    h5, h6, h7, h8, h9));
  CAF_CHECK_EQUAL(bi_pair(false, 0), tb_assign<bh2>(e1, h1, h2, h3, h4,
                                                    h5, h6, h7, h8, h9));
  CAF_CHECK_EQUAL(bi_pair(false, 1), tb_assign<bh2>(h0, e1, h2, h3, h4,
                                                    h5, h6, h7, h8, h9));
  CAF_CHECK_EQUAL(bi_pair(false, 2), tb_assign<bh2>(h0, h1, e1, h3, h4,
                                                    h5, h6, h7, h8, h9));
  CAF_CHECK_EQUAL(bi_pair(false, 3), tb_assign<bh2>(h0, h1, h2, e1, h4,
                                                    h5, h6, h7, h8, h9));
  CAF_CHECK_EQUAL(bi_pair(false, 4), tb_assign<bh2>(h0, h1, h2, h3, e1,
                                                    h5, h6, h7, h8, h9));
  CAF_CHECK_EQUAL(bi_pair(false, 5), tb_assign<bh2>(h0, h1, h2, h3, h4,
                                                    e1, h6, h7, h8, h9));
  CAF_CHECK_EQUAL(bi_pair(false, 6), tb_assign<bh2>(h0, h1, h2, h3, h4,
                                                    h5, e1, h7, h8, h9));
  CAF_CHECK_EQUAL(bi_pair(false, 7), tb_assign<bh2>(h0, h1, h2, h3, h4,
                                                    h5, h6, e1, h8, h9));
  CAF_CHECK_EQUAL(bi_pair(false, 8), tb_assign<bh2>(h0, h1, h2, h3, h4,
                                                    h5, h6, h7, e1, h9));
  CAF_CHECK_EQUAL(bi_pair(false, 9), tb_assign<bh2>(h0, h1, h2, h3, h4,
                                                    h5, h6, h7, h8, e1));
  CAF_CHECK_EQUAL(bi_pair(false, -1), tb_assign<bh2>(h0, h1, h2, h3, h4,
                                                     h5, h6, h7, h8));
}

CAF_TEST(composed_types) {
  // message type for test message #1
  auto msg_1 = tk<type_list<int>>();
  // message type for test message #1
  auto msg_2 = tk<type_list<double>>();
  // interface type a
  auto if_a = tk<type_list<replies_to<int>::with<double>,
                           replies_to<double, double>::with<int, int>>>();
  // interface type b
  auto if_b = tk<type_list<replies_to<double>::with<std::string>>>();
  // interface type c
  auto if_c = tk<type_list<replies_to<int>::with_stream<double>>>();
  // interface type b . a
  auto if_ba = tk<typed_actor<replies_to<int>::with<std::string>>>();
  // interface type b . c
  auto if_bc = tk<typed_actor<replies_to<int>::with_stream<std::string>>>();
  CAF_MESSAGE("check whether actors return the correct types");
  auto nil = tk<none_t>();
  auto dbl = tk<type_list<double>>();
  //auto dbl_stream = tk<output_stream<double>>();
  CAF_CHECK_EQUAL(res(if_a, msg_1), dbl);
  CAF_CHECK_EQUAL(res(if_a, msg_2), nil);
  //CAF_CHECK_EQUAL(res(if_c, msg_1), dbl_stream);
  CAF_MESSAGE("check types of actor compositions");
  CAF_CHECK_EQUAL(dot_op(if_b, if_a), if_ba);
  CAF_CHECK_EQUAL(dot_op(if_b, if_c), if_bc);
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
  CAF_CHECK((is_comparable<double, std::string>::value) == false);
  CAF_CHECK((is_comparable<foo, foo>::value) == false);
  CAF_CHECK((is_comparable<bar, bar>::value) == true);
  CAF_CHECK((is_comparable<double, bar>::value) == false);
  CAF_CHECK((is_comparable<bar, double>::value) == false);
  CAF_CHECK((is_comparable<baz, baz>::value) == true);
  CAF_CHECK((is_comparable<double, baz>::value) == false);
  CAF_CHECK((is_comparable<baz, double>::value) == false);
  CAF_CHECK((is_comparable<std::string, baz>::value) == false);
  CAF_CHECK((is_comparable<baz, std::string>::value) == false);
}
