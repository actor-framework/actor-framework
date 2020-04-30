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

#define CAF_SUITE make_config_value_field

#include "caf/make_config_value_field.hpp"

#include "core-test.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/config_option_set.hpp"
#include "caf/config_value_object_access.hpp"

using namespace caf;

namespace {

struct foobar {
  int foo = 0;
  std::string bar;

  foobar() = default;
  foobar(int foo, std::string bar) : foo(foo), bar(std::move(bar)) {
    // nop
  }
};

std::string to_string(const foobar& x) {
  return deep_to_string(std::forward_as_tuple(x.foo, x.bar));
}

bool operator==(const foobar& x, const foobar& y) {
  return x.foo == y.foo && x.bar == y.bar;
}

bool foo_valid(const int& x) {
  return x >= 0;
}

int get_foo_fun(foobar x) {
  return x.foo;
}

void set_foo_fun(foobar& x, const int& value) {
  x.foo = value;
}

struct get_foo_t {
  int operator()(const foobar& x) const noexcept {
    return x.foo;
  }
};

struct set_foo_t {
  int& operator()(foobar& x, int value) const noexcept {
    x.foo = value;
    return x.foo;
  }
};

struct foobar_trait {
  using object_type = foobar;

  static std::string type_name() {
    return "foobar";
  }

  static span<config_value_field<object_type>*> fields() {
    static auto singleton = make_config_value_field_storage(
      make_config_value_field("foo", &foobar::foo, 123),
      make_config_value_field("bar", &foobar::bar));
    return singleton.fields();
  }
};

struct foobar_foobar {
  foobar x;
  foobar y;
  foobar_foobar() = default;
  foobar_foobar(foobar x, foobar y) : x(x), y(y) {
    // nop
  }
};

std::string to_string(const foobar_foobar& x) {
  return deep_to_string(std::forward_as_tuple(x.x, x.y));
}

bool operator==(const foobar_foobar& x, const foobar_foobar& y) {
  return x.x == y.x && x.y == y.y;
}

struct foobar_foobar_trait {
  using object_type = foobar_foobar;

  static std::string type_name() {
    return "foobar-foobar";
  }

  static span<config_value_field<object_type>*> fields() {
    static auto singleton = make_config_value_field_storage(
      make_config_value_field("x", &foobar_foobar::x),
      make_config_value_field("y", &foobar_foobar::y));
    return singleton.fields();
  }
};

struct fixture {
  get_foo_t get_foo;

  set_foo_t set_foo;

  config_option_set opts;

  void test_foo_field(config_value_field<foobar>& foo_field) {
    foobar x;
    CAF_CHECK_EQUAL(foo_field.name(), "foo");
    CAF_REQUIRE(foo_field.has_default());
    CAF_CHECK_EQUAL(foo_field.get(x), config_value(0));
    foo_field.set_default(x);
    CAF_CHECK_EQUAL(foo_field.get(x), config_value(42));
    CAF_CHECK(!foo_field.valid_input(config_value(1.)));
    CAF_CHECK(!foo_field.valid_input(config_value(-1)));
    CAF_CHECK(!foo_field.set(x, config_value(-1)));
    string_view input = "123";
    string_parser_state ps{input.begin(), input.end()};
    foo_field.parse_cli(ps, x);
    CAF_CHECK_EQUAL(ps.code, pec::success);
    CAF_CHECK_EQUAL(foo_field.get(x), config_value(123));
  }

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
struct config_value_access<foobar> : config_value_object_access<foobar_trait> {
};

template <>
struct config_value_access<foobar_foobar>
  : config_value_object_access<foobar_foobar_trait> {};

} // namespace caf

CAF_TEST_FIXTURE_SCOPE(make_config_value_field_tests, fixture)

CAF_TEST(construction from pointer to member) {
  make_config_value_field("foo", &foobar::foo);
  make_config_value_field("foo", &foobar::foo, none);
  make_config_value_field("foo", &foobar::foo, none, nullptr);
  make_config_value_field("foo", &foobar::foo, 42);
  make_config_value_field("foo", &foobar::foo, 42, nullptr);
  make_config_value_field("foo", &foobar::foo, 42, foo_valid);
  make_config_value_field("foo", &foobar::foo, 42,
                          [](const int& x) { return x != 0; });
}

CAF_TEST(pointer to member access) {
  auto foo_field = make_config_value_field("foo", &foobar::foo, 42, foo_valid);
  test_foo_field(foo_field);
}

CAF_TEST(construction from getter and setter) {
  auto get_foo_lambda = [](const foobar& x) { return x.foo; };
  auto set_foo_lambda = [](foobar& x, int value) { x.foo = value; };
  make_config_value_field("foo", get_foo, set_foo);
  make_config_value_field("foo", get_foo_fun, set_foo);
  make_config_value_field("foo", get_foo_fun, set_foo_fun);
  make_config_value_field("foo", get_foo_lambda, set_foo_lambda);
}

CAF_TEST(getter and setter access) {
  auto foo_field = make_config_value_field("foo", get_foo, set_foo, 42,
                                           foo_valid);
  test_foo_field(foo_field);
}

CAF_TEST(object access from dictionary - foobar) {
  settings x;
  put(x, "my-value.bar", "hello");
  CAF_MESSAGE("without foo member");
  {
    CAF_REQUIRE(holds_alternative<foobar>(x, "my-value"));
    CAF_REQUIRE(get_if<foobar>(&x, "my-value") != caf::none);
    auto fb = get<foobar>(x, "my-value");
    CAF_CHECK_EQUAL(fb.foo, 123);
    CAF_CHECK_EQUAL(fb.bar, "hello");
  }
  CAF_MESSAGE("with foo member");
  put(x, "my-value.foo", 42);
  {
    CAF_REQUIRE(holds_alternative<foobar>(x, "my-value"));
    CAF_REQUIRE(get_if<foobar>(&x, "my-value") != caf::none);
    auto fb = get<foobar>(x, "my-value");
    CAF_CHECK_EQUAL(fb.foo, 42);
    CAF_CHECK_EQUAL(fb.bar, "hello");
  }
}

CAF_TEST(object access from dictionary - foobar_foobar) {
  settings x;
  put(x, "my-value.x.foo", 1);
  put(x, "my-value.x.bar", "hello");
  put(x, "my-value.y.bar", "world");
  CAF_REQUIRE(holds_alternative<foobar_foobar>(x, "my-value"));
  CAF_REQUIRE(get_if<foobar_foobar>(&x, "my-value") != caf::none);
  auto fbfb = get<foobar_foobar>(x, "my-value");
  CAF_CHECK_EQUAL(fbfb.x.foo, 1);
  CAF_CHECK_EQUAL(fbfb.x.bar, "hello");
  CAF_CHECK_EQUAL(fbfb.y.foo, 123);
  CAF_CHECK_EQUAL(fbfb.y.bar, "world");
}

CAF_TEST(object access from CLI arguments - foobar) {
  opts.add<foobar>("value,v", "some value");
  CAF_CHECK_EQUAL(read<foobar>({"--value={foo = 1, bar = hello}"}),
                  foobar(1, "hello"));
  CAF_CHECK_EQUAL(read<foobar>({"-v{bar = \"hello\"}"}), foobar(123, "hello"));
  CAF_CHECK_EQUAL(read<foobar>({"-v", "{foo = 1, bar =hello ,}"}),
                  foobar(1, "hello"));
}

CAF_TEST(object access from CLI arguments - foobar_foobar) {
  using fbfb = foobar_foobar;
  opts.add<fbfb>("value,v", "some value");
  CAF_CHECK_EQUAL(read<fbfb>({"-v{x={bar = hello},y={foo=1,bar=world!},}"}),
                  fbfb({123, "hello"}, {1, "world!"}));
}

namespace {

constexpr const char* config_text = R"__(
arg1 = {
  foo = 42
  bar = "Don't panic!"
}
arg2 = {
  x = {
    foo = 1
    bar = "hello"
  }
  y = {
    foo = 2
    bar = "world"
  }
}
)__";

struct test_config : actor_system_config {
  test_config() {
    opt_group{custom_options_, "global"}
      .add(fb, "arg1,1", "some foobar")
      .add(fbfb, "arg2,2", "somme foobar-foobar");
  }
  foobar fb;
  foobar_foobar fbfb;
};

} // namespace

CAF_TEST(object access from actor system config - file input) {
  test_config cfg;
  std::istringstream in{config_text};
  if (auto err = cfg.parse(0, nullptr, in))
    CAF_FAIL("cfg.parse failed: " << err);
  CAF_CHECK_EQUAL(cfg.fb.foo, 42);
  CAF_CHECK_EQUAL(cfg.fb.bar, "Don't panic!");
  CAF_CHECK_EQUAL(cfg.fbfb.x.foo, 1);
  CAF_CHECK_EQUAL(cfg.fbfb.y.foo, 2);
  CAF_CHECK_EQUAL(cfg.fbfb.x.bar, "hello");
  CAF_CHECK_EQUAL(cfg.fbfb.y.bar, "world");
}

CAF_TEST(object access from actor system config - file input and arguments) {
  std::vector<std::string> args{
    "-2",
    "{y = {bar = CAF, foo = 20}, x = {foo = 10, bar = hello}}",
  };
  test_config cfg;
  std::istringstream in{config_text};
  if (auto err = cfg.parse(std::move(args), in))
    CAF_FAIL("cfg.parse failed: " << err);
  CAF_CHECK_EQUAL(cfg.fb.foo, 42);
  CAF_CHECK_EQUAL(cfg.fb.bar, "Don't panic!");
  CAF_CHECK_EQUAL(cfg.fbfb.x.foo, 10);
  CAF_CHECK_EQUAL(cfg.fbfb.y.foo, 20);
  CAF_CHECK_EQUAL(cfg.fbfb.x.bar, "hello");
  CAF_CHECK_EQUAL(cfg.fbfb.y.bar, "CAF");
}

CAF_TEST_FIXTURE_SCOPE_END()
