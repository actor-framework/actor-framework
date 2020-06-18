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
#include "caf/telemetry/counter.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/telemetry/label_view.hpp"
#include "caf/telemetry/metric_type.hpp"

using namespace caf;
using namespace caf::telemetry;

namespace {

struct test_collector {
  std::string result;

  template <class T>
  void operator()(const metric_family* family, const metric* instance,
                  const counter<T>* wrapped) {
    concat(family, instance);
    result += std::to_string(wrapped->value());
  }

  void operator()(const metric_family* family, const metric* instance,
                  const dbl_gauge* wrapped) {
    concat(family, instance);
    result += std::to_string(wrapped->value());
  }

  void operator()(const metric_family* family, const metric* instance,
                  const int_gauge* wrapped) {
    concat(family, instance);
    result += std::to_string(wrapped->value());
  }

  template <class T>
  void operator()(const metric_family* family, const metric* instance,
                  const histogram<T>* wrapped) {
    concat(family, instance);
    result += std::to_string(wrapped->sum());
  }

  void concat(const metric_family* family, const metric* instance) {
    result += '\n';
    result += family->prefix();
    result += '.';
    result += family->name();
    if (family->unit() != "1") {
      result += '.';
      result += family->unit();
    }
    if (family->is_sum())
      result += ".total";
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
  test_collector collector;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(metric_registry_tests, fixture)

CAF_TEST(registries lazily create metrics) {
  std::vector<int64_t> upper_bounds{1, 2, 4, 8};
  auto f = registry.gauge_family("caf", "running-actors", {"var1", "var2"},
                                 "How many actors are currently running?");
  auto g = registry.histogram_family("caf", "response-time", {"var1", "var2"},
                                     upper_bounds, "How long take requests?");
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
  g->get_or_add(v1)->observe(3);
  g->get_or_add(v2)->observe(7);
  CAF_CHECK_EQUAL(g->get_or_add(v1)->sum(), 3);
  CAF_CHECK_EQUAL(g->get_or_add(v1_reversed)->sum(), 3);
  CAF_CHECK_EQUAL(g->get_or_add(v2)->sum(), 7);
  CAF_CHECK_EQUAL(g->get_or_add(v2_reversed)->sum(), 7);
}

CAF_TEST(registries allow users to collect all registered metrics) {
  auto fb = registry.gauge_family("foo", "bar", {},
                                  "Some value without labels.", "seconds");
  auto sv = registry.gauge_family("some", "value", {"a", "b"},
                                  "Some (total) value with two labels.", "1",
                                  true);
  auto ov = registry.gauge_family("other", "value", {"x"},
                                  "Some (total) seconds with one label.",
                                  "seconds", true);
  auto ra = registry.gauge_family("caf", "running-actors", {"node"},
                                  "How many actors are running?");
  auto ms = registry.gauge_family("caf", "mailbox-size", {"name"},
                                  "How full is the mailbox?");
  CAF_MESSAGE("the registry always returns the same family object");
  CAF_CHECK_EQUAL(fb, registry.gauge_family("foo", "bar", {}, "", "seconds"));
  CAF_CHECK_EQUAL(sv, registry.gauge_family("some", "value", {"a", "b"}, "",
                                            "1", true));
  CAF_CHECK_EQUAL(sv, registry.gauge_family("some", "value", {"b", "a"}, "",
                                            "1", true));
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
foo.bar.seconds 123
some.value.total{a="1",b="2"} 12
some.value.total{a="2",b="1"} 21
other.value.seconds.total{x="true"} 31337
caf.running-actors{node="localhost"} 42
caf.mailbox-size{name="printer"} 3
caf.mailbox-size{name="parser"} 12)");
}

CAF_TEST(buckets for histograms are configurable via runtime settings){
  auto bounds = [](auto&& buckets) {
    std::vector<int64_t> result;
    for (auto&& bucket : buckets)
      result.emplace_back(bucket.upper_bound);
    result.pop_back();
    return result;
  };
  settings cfg;
  std::vector<int64_t> default_upper_bounds{1, 2, 4, 8};
  std::vector<int64_t> upper_bounds{1, 2, 3, 5, 7};
  std::vector<int64_t> alternative_upper_bounds{10, 20, 30};
  put(cfg, "caf.response-time.buckets", upper_bounds);
  put(cfg, "caf.response-time.var1=foo.buckets", alternative_upper_bounds);
  registry.config(&cfg);
  auto hf = registry.histogram_family("caf", "response-time", {"var1", "var2"},
                                      default_upper_bounds,
                                      "How long take requests?");
  CAF_CHECK_EQUAL(hf->config(), get_if<settings>(&cfg, "caf.response-time"));
  CAF_CHECK_EQUAL(hf->extra_setting(), upper_bounds);
  auto h1 = hf->get_or_add({{"var1", "bar"}, {"var2", "baz"}});
  CAF_CHECK_EQUAL(bounds(h1->buckets()), upper_bounds);
  auto h2 = hf->get_or_add({{"var1", "foo"}, {"var2", "bar"}});
  CAF_CHECK_NOT_EQUAL(h1, h2);
  CAF_CHECK_EQUAL(bounds(h2->buckets()), alternative_upper_bounds);
}

CAF_TEST(counter_instance is a shortcut for using the family manually) {
  auto fptr = registry.counter_family("http", "requests", {"method"},
                                      "Number of HTTP requests.", "seconds",
                                      true);
  auto count = fptr->get_or_add({{"method", "put"}});
  auto count2
    = registry.counter_instance("http", "requests", {{"method", "put"}},
                                "Number of HTTP requests.", "seconds", true);
  CAF_CHECK_EQUAL(count, count2);
}

CAF_TEST_FIXTURE_SCOPE_END()
