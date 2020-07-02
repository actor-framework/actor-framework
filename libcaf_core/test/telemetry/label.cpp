/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

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
