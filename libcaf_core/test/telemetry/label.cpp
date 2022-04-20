// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE telemetry.label

#include "caf/telemetry/label.hpp"

#include "core-test.hpp"

#include "caf/telemetry/label_view.hpp"

using namespace caf;
using namespace caf::telemetry;

namespace {

template <class T>
size_t hash(const T& x) {
  std::hash<T> f;
  return f(x);
}

} // namespace

CAF_TEST(labels wrap name and value) {
  CHECK_EQ(to_string(label{"foo", "bar"}), "foo=bar");
  label foobar{"foo", "bar"};
  CHECK_EQ(foobar.name(), "foo");
  CHECK_EQ(foobar.value(), "bar");
  CHECK_EQ(foobar.str(), "foo=bar");
  CHECK_EQ(to_string(foobar), "foo=bar");
  CHECK_EQ(foobar, label("foo", "bar"));
  CHECK_EQ(hash(foobar), hash(label("foo", "bar")));
}

CAF_TEST(labels are convertible from views) {
  label foobar{"foo", "bar"};
  label_view foobar_view{"foo", "bar"};
  CHECK_EQ(foobar, foobar_view);
  CHECK_EQ(foobar, label{foobar_view});
  CHECK_EQ(foobar.name(), foobar_view.name());
  CHECK_EQ(foobar.value(), foobar_view.value());
  CHECK_EQ(to_string(foobar), to_string(foobar_view));
  CHECK_EQ(hash(foobar), hash(foobar_view));
  CHECK_EQ(hash(foobar), hash(label{foobar_view}));
}
