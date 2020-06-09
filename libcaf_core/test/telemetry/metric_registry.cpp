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

struct collector {
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

CAF_TEST(registries allow users to collect all registered metrics) {
  registry.add_family(metric_type::int_gauge, "foo", "bar", {},
                      "Just some value without labels.", "seconds");
  registry.add_family(metric_type::int_gauge, "some", "value", {"a", "b"},
                      "Just some (total) value with two labels.", "1", true);
  registry.add_family(metric_type::int_gauge, "other", "value", {"x"},
                      "Just some (total) seconds value with a labels.",
                      "seconds", true);
  registry.add_family(metric_type::int_gauge, "caf", "running_actors", {"node"},
                      "How many actors are currently running?");
  registry.add_family(metric_type::int_gauge, "caf", "mailbox_size", {"name"},
                      "How many message are currently in mailboxes?");
  registry.int_gauge("foo", "bar", {})->value(123);
  registry.int_gauge("some", "value", {{"a", "1"}, {"b", "2"}})->value(12);
  registry.int_gauge("some", "value", {{"b", "1"}, {"a", "2"}})->value(21);
  registry.int_gauge("other", "value", {{"x", "true"}})->value(31337);
  auto running_actors = bind("caf", "running_actors");
  running_actors.int_gauge({{"node", "localhost"}})->value(42);
  auto mailbox_size = bind("caf", "mailbox_size");
  mailbox_size.int_gauge({{"name", "printer"}})->value(3);
  mailbox_size.int_gauge({{"name", "parser"}})->value(12);
  collector c;
  registry.collect(c);
  CAF_CHECK_EQUAL(c.result, R"(
foo_bar_seconds 123
some_value_total{a="1",b="2"} 12
some_value_total{a="2",b="1"} 21
other_value_seconds_total{x="true"} 31337
caf_running_actors{node="localhost"} 42
caf_mailbox_size{name="printer"} 3
caf_mailbox_size{name="parser"} 12)");
}

CAF_TEST_FIXTURE_SCOPE_END()
