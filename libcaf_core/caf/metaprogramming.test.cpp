// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/test.hpp"

#include "caf/deduce_mpi.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/interface_mismatch.hpp"
#include "caf/response_type.hpp"
#include "caf/result.hpp"

#include <cstdint>
#include <string>
#include <type_traits>
#include <typeinfo>

using namespace caf;
using namespace caf::detail;

namespace {

// misc

template <class T>
struct is_int : std::false_type {};

template <>
struct is_int<int> : std::true_type {};

} // namespace

TEST("metaprogramming") {
  using std::is_same;
  using l1 = type_list<int, float, std::string>;
  check((is_same<int, tl_at<l1, 0>::type>::value));
  check((is_same<float, tl_at<l1, 1>::type>::value));
  check((is_same<std::string, tl_at<l1, 2>::type>::value));
  check_eq(3u, tl_size<l1>::value);
  using il0 = int_list<0, 1, 2, 3, 4, 5>;
  using il1 = int_list<4, 5>;
  using il2 = il_right<il0, 2>::type;
  check((is_same<il2, il1>::value));
  /* test tl_subset_of */ {
    using list_a = type_list<int, float, double>;
    using list_b = type_list<float, int, double, std::string>;
    check((tl_subset_of_v<list_a, list_b>) );
    check(!(tl_subset_of_v<list_b, list_a>) );
    check((tl_subset_of_v<list_a, list_a>) );
    check((tl_subset_of_v<list_b, list_b>) );
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
  return std::is_same_v<T, U>;
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

TEST("typed_behavior_assignment") {
  using bh1 = typed_beh<result<double>(int), result<int, int>(double, double)>;
  // compatible handlers resulting in perfect match
  auto f1 = [=](int) { return 0.; };
  auto f2 = [=](double, double) -> result<int, int> { return {0, 0}; };
  // incompatbile handlers
  auto e1 = [=](int) { return 0.f; };
  auto e2 = [=](double, double) { return std::make_tuple(0.f, 0.f); };
  // omit one handler
  check_eq(bi_pair(false, -1), tb_assign<bh1>(f1));
  check_eq(bi_pair(false, -1), tb_assign<bh1>(f2));
  check_eq(bi_pair(false, -1), tb_assign<bh1>(e1));
  check_eq(bi_pair(false, -1), tb_assign<bh1>(e2));
  // any valid alteration of (f1, f2)
  check_eq(bi_pair(true, 2), tb_assign<bh1>(f1, f2));
  check_eq(bi_pair(true, 2), tb_assign<bh1>(f2, f1));
  // any invalid alteration of (f1, f2, e1, e2)
  check_eq(bi_pair(false, 1), tb_assign<bh1>(f1, e1));
  check_eq(bi_pair(false, 1), tb_assign<bh1>(f1, e2));
  check_eq(bi_pair(false, 0), tb_assign<bh1>(e1, f1));
  check_eq(bi_pair(false, 0), tb_assign<bh1>(e1, f2));
  check_eq(bi_pair(false, 0), tb_assign<bh1>(e1, e2));
  check_eq(bi_pair(false, 1), tb_assign<bh1>(f2, e1));
  check_eq(bi_pair(false, 1), tb_assign<bh1>(f2, e2));
  check_eq(bi_pair(false, 0), tb_assign<bh1>(e2, f1));
  check_eq(bi_pair(false, 0), tb_assign<bh1>(e2, f2));
  check_eq(bi_pair(false, 0), tb_assign<bh1>(e2, e1));
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
  check_eq(bi_pair(true, 10),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9));
  check_eq(bi_pair(false, 0),
           tb_assign<bh2>(e1, h1, h2, h3, h4, h5, h6, h7, h8, h9));
  check_eq(bi_pair(false, 1),
           tb_assign<bh2>(h0, e1, h2, h3, h4, h5, h6, h7, h8, h9));
  check_eq(bi_pair(false, 2),
           tb_assign<bh2>(h0, h1, e1, h3, h4, h5, h6, h7, h8, h9));
  check_eq(bi_pair(false, 3),
           tb_assign<bh2>(h0, h1, h2, e1, h4, h5, h6, h7, h8, h9));
  check_eq(bi_pair(false, 4),
           tb_assign<bh2>(h0, h1, h2, h3, e1, h5, h6, h7, h8, h9));
  check_eq(bi_pair(false, 5),
           tb_assign<bh2>(h0, h1, h2, h3, h4, e1, h6, h7, h8, h9));
  check_eq(bi_pair(false, 6),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, e1, h7, h8, h9));
  check_eq(bi_pair(false, 7),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, h6, e1, h8, h9));
  check_eq(bi_pair(false, 8),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, h6, h7, e1, h9));
  check_eq(bi_pair(false, 9),
           tb_assign<bh2>(h0, h1, h2, h3, h4, h5, h6, h7, h8, e1));
  check_eq(bi_pair(false, -1),
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

TEST("is_comparable") {
  check((is_comparable_v<double, std::string>) == false);
  check((is_comparable_v<foo, foo>) == false);
  check((is_comparable_v<bar, bar>) == true);
  check((is_comparable_v<double, bar>) == false);
  check((is_comparable_v<bar, double>) == false);
  check((is_comparable_v<baz, baz>) == true);
  check((is_comparable_v<double, baz>) == false);
  check((is_comparable_v<baz, double>) == false);
  check((is_comparable_v<std::string, baz>) == false);
  check((is_comparable_v<baz, std::string>) == false);
}
