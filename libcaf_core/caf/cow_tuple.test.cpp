// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/cow_tuple.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"

using std::make_tuple;
using std::string;
using std::tuple;

using namespace caf;

namespace {

template <class... Ts>
caf::byte_buffer serialize(const Ts&... xs) {
  caf::byte_buffer buf;
  caf::binary_serializer sink{buf};
  if (!(sink.apply(xs) && ...))
    test::runnable::current().fail("serialization failed: {}",
                                   sink.get_error());
  return buf;
}

template <class... Ts>
void deserialize(const caf::byte_buffer& buf, Ts&... xs) {
  caf::binary_deserializer source{buf};
  if (!(source.apply(xs) && ...))
    test::runnable::current().fail("deserialization failed: {}",
                                   source.get_error());
}

template <class T>
T roundtrip(const T& x) {
  T result;
  deserialize(serialize(x), result);
  return result;
}

} // namespace

TEST("default_construction") {
  cow_tuple<string, string> x;
  check_eq(x.unique(), true);
  check_eq(get<0>(x), "");
  check_eq(get<1>(x), "");
}

TEST("value_construction") {
  cow_tuple<int, int> x{1, 2};
  check_eq(x.unique(), true);
  check_eq(get<0>(x), 1);
  check_eq(get<1>(x), 2);
  check_eq(x, make_cow_tuple(1, 2));
}

TEST("copy_construction") {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{x};
  check_eq(x, y);
  check_eq(x.ptr(), y.ptr());
  check_eq(x.unique(), false);
  check_eq(y.unique(), false);
}

TEST("move_construction") {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{std::move(x)};
  check_eq(x.ptr(), nullptr); // NOLINT(bugprone-use-after-move)
  check_eq(y, make_tuple(1, 2));
  check_eq(y.unique(), true);
}

TEST("copy_assignment") {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{3, 4};
  check_ne(x, y);
  x = y;
  check_eq(x, y);
  check_eq(x.ptr(), y.ptr());
  check_eq(x.unique(), false);
  check_eq(y.unique(), false);
}

TEST("move_assignment") {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{3, 4};
  check_ne(x, y);
  x = std::move(y);
  check_eq(x, make_tuple(3, 4));
  check_eq(x.unique(), true);
}

TEST("make_cow_tuple") {
  cow_tuple<int, int> x{1, 2};
  auto y = make_cow_tuple(1, 2);
  check_eq(x, y);
  check_eq(x.unique(), true);
  check_eq(y.unique(), true);
}

TEST("unsharing") {
  auto x = make_cow_tuple(string{"old"}, string{"school"});
  auto y = x;
  check_eq(x.unique(), false);
  check_eq(y.unique(), false);
  get<0>(y.unshared()) = "new";
  check_eq(x.unique(), true);
  check_eq(y.unique(), true);
  check_eq(x.data(), make_tuple("old", "school"));
  check_eq(y.data(), make_tuple("new", "school"));
}

TEST("to_string") {
  auto x = make_cow_tuple(1, string{"abc"});
  check_eq(deep_to_string(x), R"__([1, "abc"])__");
}

TEST("serialization") {
  auto x = make_cow_tuple(1, 2, 3);
  auto y = roundtrip(x);
  check_eq(x, y);
  check_eq(x.unique(), true);
  check_eq(y.unique(), true);
  check_ne(x.ptr(), y.ptr());
}
