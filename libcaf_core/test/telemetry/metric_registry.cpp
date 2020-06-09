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

#define CAF_SUITE telemetry.metric_registry

#include "caf/telemetry/metric_registry.hpp"

#include "caf/test/dsl.hpp"

#include "caf/string_view.hpp"
#include "caf/telemetry/int_gauge.hpp"
#include "caf/telemetry/label_view.hpp"
#include "caf/telemetry/metric_type.hpp"

using namespace caf;
using namespace caf::telemetry;

namespace {

struct fixture {
  metric_registry registry;

  auto bind(string_view prefix, string_view name) {
    struct bound {
      fixture* self_;
      string_view prefix_;
      string_view name_;
      auto int_gauge(std::vector<label_view> labels) {
        return self_->registry.int_gauge(prefix_, name_, std::move(labels));
      }
    };
    return bound{this, prefix, name};
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(metric_registry_tests, fixture)

CAF_TEST(registries lazily create metrics) {
  registry.add_family(metric_type::int_gauge, "caf", "running_actors",
                      {"var1", "var2"},
                      "How many actors are currently running?");
  auto var = bind("caf", "running_actors");
  std::vector<label_view> v1{{"var1", "foo"}, {"var2", "bar"}};
  std::vector<label_view> v1_reversed{{"var2", "bar"}, {"var1", "foo"}};
  std::vector<label_view> v2{{"var1", "bar"}, {"var2", "foo"}};
  std::vector<label_view> v2_reversed{{"var2", "foo"}, {"var1", "bar"}};
  var.int_gauge(v1)->value(42);
  var.int_gauge(v2)->value(23);
  CAF_CHECK_EQUAL(var.int_gauge(v1)->value(), 42);
  CAF_CHECK_EQUAL(var.int_gauge(v1_reversed)->value(), 42);
  CAF_CHECK_EQUAL(var.int_gauge(v2)->value(), 23);
  CAF_CHECK_EQUAL(var.int_gauge(v2_reversed)->value(), 23);
}

CAF_TEST_FIXTURE_SCOPE_END()
