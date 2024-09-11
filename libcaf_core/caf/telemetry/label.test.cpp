// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/telemetry/label.hpp"

#include "caf/test/test.hpp"

#include "caf/telemetry/label_view.hpp"

using namespace caf;
using namespace caf::telemetry;

namespace {

template <class T>
size_t hash_of(const T& x) {
  std::hash<T> f;
  return f(x);
}

} // namespace

TEST("labels wrap name and value") {
  check_eq(to_string(label{"foo", "bar"}), "foo=bar");
  label foobar{"foo", "bar"};
  check_eq(foobar.name(), "foo");
  check_eq(foobar.value(), "bar");
  check_eq(foobar.str(), "foo=bar");
  check_eq(to_string(foobar), "foo=bar");
  check_eq(foobar, label("foo", "bar"));
  check_eq(hash_of(foobar), hash_of(label("foo", "bar")));
}

TEST("labels are convertible from views") {
  label foobar{"foo", "bar"};
  label_view foobar_view{"foo", "bar"};
  check_eq(foobar, foobar_view);
  check_eq(foobar, label{foobar_view});
  check_eq(foobar.name(), foobar_view.name());
  check_eq(foobar.value(), foobar_view.value());
  check_eq(to_string(foobar), to_string(foobar_view));
  check_eq(hash_of(foobar), hash_of(foobar_view));
  check_eq(hash_of(foobar), hash_of(label{foobar_view}));
}
