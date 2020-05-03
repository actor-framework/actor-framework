/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE config_value_adaptor

#include "caf/config_value_adaptor.hpp"

#include "core-test.hpp"

#include "caf/config_value_adaptor_access.hpp"

using namespace caf;

namespace {

// We want to configure this type as follows:
// my-duration = {
//   count = 1
//   resolution = "s"
// }
struct my_duration {
public:
  constexpr my_duration() noexcept : ns_(0) {
    // nop
  }

  constexpr my_duration(const my_duration&) noexcept = default;

  my_duration& operator=(const my_duration&) noexcept = default;

  int64_t ns() const {
    return ns_;
  }

  int64_t us() const {
    return ns() / 1000;
  }

  int64_t ms() const {
    return us() / 1000;
  }

  int64_t s() const {
    return ms() / 1000;
  }

  static my_duration from_ns(int64_t count) {
    my_duration result;
    result.ns_ = count;
    return result;
  }

  static my_duration from_us(int64_t count) {
    return from_ns(count * 1000);
  }

  static my_duration from_ms(int64_t count) {
    return from_us(count * 1000);
  }

  static my_duration from_s(int64_t count) {
    return from_ms(count * 1000);
  }

private:
  int64_t ns_;
};

bool operator==(my_duration x, my_duration y) {
  return x.ns() == y.ns();
}

std::string to_string(my_duration x) {
  return std::to_string(x.ns()) + "ns";
}

struct my_duration_adaptor {
  using value_type = my_duration;

  using tuple_type = std::tuple<int64_t, std::string>;

  static std::string type_name() noexcept {
    return "my-duration";
  }

  static bool resolution_valid(const std::string& str) {
    static constexpr string_view whitelist[] = {"s", "ms", "us", "ns"};
    auto matches = [&](string_view x) { return str == x; };
    return std::any_of(std::begin(whitelist), std::end(whitelist), matches);
  }

  static config_value_adaptor<int64_t, std::string>& adaptor_ref() {
    static auto singleton = make_config_value_adaptor(
      make_config_value_adaptor_field<int64_t>("count"),
      make_config_value_adaptor_field<std::string>("resolution", none,
                                                   resolution_valid));
    return singleton;
  }

  static void convert(const value_type& src, tuple_type& dst) {
    int count = src.ns();
    if (count / 1000 != 0) {
      dst = std::tie(count, "ns");
      return;
    }
    count /= 1000;
    if (count / 1000 != 0) {
      dst = std::tie(count, "us");
      return;
    }
    count /= 1000;
    if (count / 1000 != 0) {
      dst = std::tie(count, "ms");
      return;
    }
    count /= 1000;
    dst = std::tie(count, "s");
  }

  static void convert(const tuple_type& src, value_type& dst) {
    auto count = std::get<0>(src);
    const auto& resolution = std::get<1>(src);
    if (resolution == "ns")
      dst = my_duration::from_ns(count);
    else if (resolution == "us")
      dst = my_duration::from_us(count);
    else if (resolution == "ms")
      dst = my_duration::from_ms(count);
    else
      dst = my_duration::from_s(count);
  }
};

struct fixture {
  config_option_set opts;

  template <class T>
  expected<T> read(std::vector<std::string> args) {
    settings cfg;
    auto res = opts.parse(cfg, args);
    if (res.first != pec::success)
      return make_error(res.first, *res.second);
    auto x = get_if<T>(&cfg, "value");
    if (x == none)
      return sec::invalid_argument;
    return *x;
  }
};

} // namespace

namespace caf {

template <>
struct config_value_access<my_duration>
  : config_value_adaptor_access<my_duration_adaptor> {};

} // namespace caf

CAF_TEST_FIXTURE_SCOPE(config_value_adaptor_tests, fixture)

CAF_TEST(holds_alternative) {
  auto make_value = [](int64_t count, std::string resolution) {
    settings x;
    put(x, "count", count);
    put(x, "resolution", std::move(resolution));
    return config_value{std::move(x)};
  };
  CAF_CHECK(holds_alternative<my_duration>(make_value(1, "s")));
  CAF_CHECK(holds_alternative<my_duration>(make_value(1, "ms")));
  CAF_CHECK(holds_alternative<my_duration>(make_value(1, "us")));
  CAF_CHECK(holds_alternative<my_duration>(make_value(1, "ns")));
  CAF_CHECK(!holds_alternative<my_duration>(make_value(1, "foo")));
}

CAF_TEST(access from dictionary) {
  settings x;
  put(x, "value.count", 42);
  put(x, "value.resolution", "s");
  auto value = x["value"];
  CAF_REQUIRE(holds_alternative<my_duration>(value));
  CAF_CHECK_EQUAL(get_if<my_duration>(&value), my_duration::from_s(42));
  CAF_CHECK_EQUAL(get<my_duration>(value), my_duration::from_s(42));
}

namespace {

constexpr const char* config_text = R"__(
max-delay = {
  count = 123
  resolution = "s"
}
)__";

struct test_config : actor_system_config {
  test_config() {
    opt_group{custom_options_, "global"}.add(max_delay, "max-delay,m",
                                             "maximum delay");
  }
  my_duration max_delay;
};

} // namespace

CAF_TEST(adaptor access from actor system config - file input) {
  test_config cfg;
  std::istringstream in{config_text};
  if (auto err = cfg.parse(0, nullptr, in))
    CAF_FAIL("cfg.parse failed: " << err);
  CAF_CHECK_EQUAL(cfg.max_delay, my_duration::from_s(123));
}

CAF_TEST(adaptor access from actor system config - file input and arguments) {
  std::vector<std::string> args{
    "--max-delay={count = 20, resolution = ms}",
  };
  test_config cfg;
  std::istringstream in{config_text};
  if (auto err = cfg.parse(std::move(args), in))
    CAF_FAIL("cfg.parse failed: " << err);
  CAF_CHECK_EQUAL(cfg.max_delay, my_duration::from_ms(20));
}

CAF_TEST_FIXTURE_SCOPE_END()
