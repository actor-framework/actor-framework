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

struct test_collector {
  std::string result;

  void operator()(const metric_family* family, const metric* instance,
                  const int_gauge* wrapped) {
    result += '\n';
    result += family->prefix();
    result += '_';
    result += family->name();
    if (family->unit() != "1") {
      result += '_';
      result += family->unit();
    }
    if (family->is_sum())
      result += "_total";
    if (!instance->labels().empty()) {
      result += '{';
      auto i = instance->labels().begin();
      concat(*i++);
      while (i != instance->labels().end()) {
        result += ',';
        concat(*i++);
      }
      result += '}';
    }
    result += ' ';
    result += std::to_string(wrapped->value());
  }

  void concat(const label& lbl) {
    result.insert(result.end(), lbl.name().begin(), lbl.name().end());
    result += "=\"";
    result.insert(result.end(), lbl.value().begin(), lbl.value().end());
    result += '"';
  }
};

struct fixture {
  using ig = int_gauge;
  metric_registry registry;
  test_collector collector;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(metric_registry_tests, fixture)

CAF_TEST(registries lazily create metrics) {
  auto f = registry.family<ig>("caf", "running_actors", {"var1", "var2"},
                               "How many actors are currently running?");
  std::vector<label_view> v1{{"var1", "foo"}, {"var2", "bar"}};
  std::vector<label_view> v1_reversed{{"var2", "bar"}, {"var1", "foo"}};
  std::vector<label_view> v2{{"var1", "bar"}, {"var2", "foo"}};
  std::vector<label_view> v2_reversed{{"var2", "foo"}, {"var1", "bar"}};
  f->get_or_add(v1)->value(42);
  f->get_or_add(v2)->value(23);
  CAF_CHECK_EQUAL(f->get_or_add(v1)->value(), 42);
  CAF_CHECK_EQUAL(f->get_or_add(v1_reversed)->value(), 42);
  CAF_CHECK_EQUAL(f->get_or_add(v2)->value(), 23);
  CAF_CHECK_EQUAL(f->get_or_add(v2_reversed)->value(), 23);
}

CAF_TEST(registries allow users to collect all registered metrics) {
  auto fb = registry.family<ig>("foo", "bar", {}, "Some value without labels.",
                                "seconds");
  auto sv = registry.family<ig>("some", "value", {"a", "b"},
                                "Some (total) value with two labels.", "1",
                                true);
  auto ov = registry.family<ig>("other", "value", {"x"},
                                "Some (total) seconds with one label.",
                                "seconds", true);
  auto ra = registry.family<ig>("caf", "running_actors", {"node"},
                                "How many actors are running?");
  auto ms = registry.family<ig>("caf", "mailbox_size", {"name"},
                                "How full is the mailbox?");
  CAF_MESSAGE("the registry always returns the same family object");
  CAF_CHECK_EQUAL(fb, registry.family<ig>("foo", "bar", {}, "", "seconds"));
  CAF_CHECK_EQUAL(sv, registry.family<ig>("some", "value", {"a", "b"}, "", "1",
                                          true));
  CAF_CHECK_EQUAL(sv, registry.family<ig>("some", "value", {"b", "a"}, "", "1",
                                          true));
  CAF_MESSAGE("families always return the same metric object for given labels");
  CAF_CHECK_EQUAL(fb->get_or_add({}), fb->get_or_add({}));
  CAF_CHECK_EQUAL(sv->get_or_add({{"a", "1"}, {"b", "2"}}),
                  sv->get_or_add({{"b", "2"}, {"a", "1"}}));
  CAF_MESSAGE("collectors can observe all metrics in the registry");
  fb->get_or_add({})->inc(123);
  sv->get_or_add({{"a", "1"}, {"b", "2"}})->value(12);
  sv->get_or_add({{"b", "1"}, {"a", "2"}})->value(21);
  ov->get_or_add({{"x", "true"}})->value(31337);
  ra->get_or_add({{"node", "localhost"}})->value(42);
  ms->get_or_add({{"name", "printer"}})->value(3);
  ms->get_or_add({{"name", "parser"}})->value(12);
  registry.collect(collector);
  CAF_CHECK_EQUAL(collector.result, R"(
foo_bar_seconds 123
some_value_total{a="1",b="2"} 12
some_value_total{a="2",b="1"} 21
other_value_seconds_total{x="true"} 31337
caf_running_actors{node="localhost"} 42
caf_mailbox_size{name="printer"} 3
caf_mailbox_size{name="parser"} 12)");
}

CAF_TEST_FIXTURE_SCOPE_END()
