// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/telemetry/metric_registry.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/telemetry/counter.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/telemetry/label_view.hpp"
#include "caf/telemetry/metric_type.hpp"

using namespace caf;
using namespace caf::telemetry;
using namespace std::literals;

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
  metric_registry reg;
  test_collector collector;
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("registries lazily create metrics") {
  std::vector<int64_t> upper_bounds{1, 2, 4, 8};
  auto f = reg.gauge_family("caf", "running-actors", {"var1", "var2"},
                            "How many actors are currently running?");
  auto g = reg.histogram_family("caf", "response-time", {"var1", "var2"},
                                upper_bounds, "How long take requests?");
  std::vector<label_view> v1{{"var1", "foo"}, {"var2", "bar"}};
  std::vector<label_view> v1_reversed{{"var2", "bar"}, {"var1", "foo"}};
  std::vector<label_view> v2{{"var1", "bar"}, {"var2", "foo"}};
  std::vector<label_view> v2_reversed{{"var2", "foo"}, {"var1", "bar"}};
  f->get_or_add(v1)->value(42);
  f->get_or_add(v2)->value(23);
  check_eq(f->get_or_add(v1)->value(), 42);
  check_eq(f->get_or_add(v1_reversed)->value(), 42);
  check_eq(f->get_or_add(v2)->value(), 23);
  check_eq(f->get_or_add(v2_reversed)->value(), 23);
  g->get_or_add(v1)->observe(3);
  g->get_or_add(v2)->observe(7);
  check_eq(g->get_or_add(v1)->sum(), 3);
  check_eq(g->get_or_add(v1_reversed)->sum(), 3);
  check_eq(g->get_or_add(v2)->sum(), 7);
  check_eq(g->get_or_add(v2_reversed)->sum(), 7);
}

TEST("registries allow users to collect all registered metrics") {
  auto fb = reg.gauge_family("foo", "bar", {}, "Some value without labels.",
                             "seconds");
  auto sv = reg.gauge_family("some", "value", {"a", "b"},
                             "Some (total) value with two labels.", "1", true);
  auto ov = reg.gauge_family("other", "value", {"x"},
                             "Some (total) seconds with one label.", "seconds",
                             true);
  auto ra = reg.gauge_family("caf", "running-actors", {"node"},
                             "How many actors are running?");
  auto ms = reg.gauge_family("caf", "mailbox-size", {"name"},
                             "How full is the mailbox?");
  SECTION("the registry always returns the same family object") {
    check_eq(fb, reg.gauge_family("foo", "bar", {}, "", "seconds"));
    check_eq(sv, reg.gauge_family("some", "value", {"a", "b"}, "", "1", true));
    check_eq(sv, reg.gauge_family("some", "value", {"b", "a"}, "", "1", true));
  }
  SECTION("families always return the same metric object for given labels") {
    check_eq(fb->get_or_add({}), fb->get_or_add({}));
    check_eq(sv->get_or_add({{"a", "1"}, {"b", "2"}}),
             sv->get_or_add({{"b", "2"}, {"a", "1"}}));
  }
  SECTION("collectors can observe all metrics in the registry") {
    fb->get_or_add({})->inc(123);
    sv->get_or_add({{"a", "1"}, {"b", "2"}})->value(12);
    sv->get_or_add({{"b", "1"}, {"a", "2"}})->value(21);
    ov->get_or_add({{"x", "true"}})->value(31337);
    ra->get_or_add({{"node", "localhost"}})->value(42);
    ms->get_or_add({{"name", "printer"}})->value(3);
    ms->get_or_add({{"name", "parser"}})->value(12);
    reg.collect(collector);
    check_eq(collector.result, R"(
foo.bar.seconds 123
some.value.total{a="1",b="2"} 12
some.value.total{a="2",b="1"} 21
other.value.seconds.total{x="true"} 31337
caf.running-actors{node="localhost"} 42
caf.mailbox-size{name="printer"} 3
caf.mailbox-size{name="parser"} 12)");
  }
}

TEST("buckets for histograms are configurable via runtime settings") {
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
  reg.config(&cfg);
  auto hf = reg.histogram_family("caf", "response-time", {"var1", "var2"},
                                 default_upper_bounds,
                                 "How long take requests?");
  check_eq(hf->config(), get_if<settings>(&cfg, "caf.response-time"));
  check_eq(hf->extra_setting(), upper_bounds);
  auto h1 = hf->get_or_add({{"var1", "bar"}, {"var2", "baz"}});
  check_eq(bounds(h1->buckets()), upper_bounds);
  auto h2 = hf->get_or_add({{"var1", "foo"}, {"var2", "bar"}});
  check_ne(h1, h2);
  check_eq(bounds(h2->buckets()), alternative_upper_bounds);
}

SCENARIO("instance methods provide a shortcut for using the family manually") {
  GIVEN("an int counter family with at least one label dimension") {
    WHEN("calling counter_instance on the registry") {
      THEN("calling get_or_add on the family object returns the same pointer") {
        auto fp = reg.counter_family("http", "requests", {"method"},
                                     "Number of HTTP requests.", "seconds",
                                     true);
        auto p1 = fp->get_or_add({{"method", "put"}});
        auto p2 = reg.counter_instance("http", "requests", {{"method", "put"}},
                                       "Number of HTTP requests.", "seconds",
                                       true);
        check_eq(p1, p2);
      }
    }
  }
  GIVEN("an int gauge family with at least one label dimension") {
    WHEN("calling gauge_instance on the registry") {
      THEN("calling get_or_add on the family object returns the same pointer") {
        auto fp = reg.gauge_family("db", "pending", {"operation"},
                                   "Pending DB operations.");
        auto p1 = fp->get_or_add({{"operation", "update"}});
        auto p2 = reg.gauge_instance("db", "pending", {{"operation", "update"}},
                                     "Pending DB operations.");
        check_eq(p1, p2);
      }
    }
  }
  GIVEN("an int histogram family with at least one label dimension") {
    WHEN("calling histogram_instance on the registry") {
      THEN("calling get_or_add on the family object returns the same pointer") {
        std::vector<int64_t> upper_bounds{1, 2, 3, 5, 7};
        auto fp = reg.histogram_family("db", "query-results", {"operation"},
                                       upper_bounds, "Results per query.");
        auto p1 = fp->get_or_add({{"operation", "update"}});
        auto p2 = reg.histogram_instance("db", "query-results",
                                         {{"operation", "update"}},
                                         upper_bounds, "Results per query.");
        check_eq(p1, p2);
      }
    }
  }
  GIVEN("a double counter family with at least one label dimension") {
    WHEN("calling counter_instance on the registry") {
      THEN("calling get_or_add on the family object returns the same pointer") {
        auto fp = reg.counter_family<double>("db", "cpu-usage", {"operation"},
                                             "Total CPU time by query type.",
                                             "seconds", true);
        auto p1 = fp->get_or_add({{"operation", "update"}});
        auto p2 = reg.counter_instance<double>("db", "cpu-usage",
                                               {{"operation", "update"}},
                                               "Total CPU time by query type.",
                                               "seconds", true);
        check_eq(p1, p2);
      }
    }
  }
  GIVEN("a double gauge family with at least one label dimension") {
    WHEN("calling gauge_instance on the registry") {
      THEN("calling get_or_add on the family object returns the same pointer") {
        auto fp = reg.gauge_family<double>("sensor", "water-level",
                                           {"location"},
                                           "Water level by location.");
        auto p1 = fp->get_or_add({{"location", "tank-1"}});
        auto p2 = reg.gauge_instance<double>("sensor", "water-level",
                                             {{"location", "tank-1"}},
                                             "Water level by location.");
        check_eq(p1, p2);
      }
    }
  }
  GIVEN("a double histogram family with at least one label dimension") {
    WHEN("calling histogram_instance on the registry") {
      THEN("calling get_or_add on the family object returns the same pointer") {
        std::vector<double> upper_bounds{1, 2, 3, 5, 7};
        auto fp = reg.histogram_family<double>("db", "query-duration",
                                               {"operation"}, upper_bounds,
                                               "Query processing time.");
        auto p1 = fp->get_or_add({{"operation", "update"}});
        auto p2 = reg.histogram_instance<double>("db", "query-duration",
                                                 {{"operation", "update"}},
                                                 upper_bounds,
                                                 "Query processing time.");
        check_eq(p1, p2);
      }
    }
  }
}

SCENARIO("metric registries can merge families from other registries") {
  GIVEN("a registry with some metrics") {
    metric_registry tmp;
    auto foo_bar = tmp.counter_singleton("foo", "bar", "test metric");
    auto bar_foo = tmp.counter_singleton("bar", "foo", "test metric");
    WHEN("merging the registry into another one") {
      reg.merge(tmp);
      THEN("all metrics move into the new location") {
        check_eq(foo_bar, reg.counter_singleton("foo", "bar", "test metric"));
        check_eq(bar_foo, reg.counter_singleton("bar", "foo", "test metric"));
        tmp.collect(collector);
        check(collector.result.empty());
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

namespace {

struct foo_state {
  static constexpr const char* name = "foo";

  behavior make_behavior() {
    return {};
  }
};

} // namespace

TEST("enabling actor metrics per config creates metric instances") {
  actor_system_config cfg;
  put(cfg.content, "caf.scheduuler.max-threads", 1);
  put(cfg.content, "caf.metrics.filters.actors.includes", std::vector{"foo"s});
  actor_system sys{cfg};
  auto hdl = sys.spawn(actor_from_state<foo_state>);
  scoped_actor self{sys};
  self->wait_for(hdl);
  test_collector collector;
  sys.metrics().collect(collector);
  check_ne(collector.result.find(R"(caf.actor.mailbox-size{name="foo"})"),
           std::string::npos);
}
