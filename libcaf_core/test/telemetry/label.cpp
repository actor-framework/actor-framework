// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE telemetry.label

#include "caf/telemetry/label.hpp"

#include "caf/test/dsl.hpp"

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
  CAF_CHECK_EQUAL(to_string(label{"foo", "bar"}), "foo=bar");
  label foobar{"foo", "bar"};
  CAF_CHECK_EQUAL(foobar.name(), "foo");
  CAF_CHECK_EQUAL(foobar.value(), "bar");
  CAF_CHECK_EQUAL(foobar.str(), "foo=bar");
  CAF_CHECK_EQUAL(to_string(foobar), "foo=bar");
  CAF_CHECK_EQUAL(foobar, label("foo", "bar"));
  CAF_CHECK_EQUAL(hash(foobar), hash(label("foo", "bar")));
}

CAF_TEST(labels are convertible from views) {
  label foobar{"foo", "bar"};
  label_view foobar_view{"foo", "bar"};
  CAF_CHECK_EQUAL(foobar, foobar_view);
  CAF_CHECK_EQUAL(foobar, label{foobar_view});
  CAF_CHECK_EQUAL(foobar.name(), foobar_view.name());
  CAF_CHECK_EQUAL(foobar.value(), foobar_view.value());
  CAF_CHECK_EQUAL(to_string(foobar), to_string(foobar_view));
  CAF_CHECK_EQUAL(hash(foobar), hash(foobar_view));
  CAF_CHECK_EQUAL(hash(foobar), hash(label{foobar_view}));
}
