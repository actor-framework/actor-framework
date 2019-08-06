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

#define CAF_SUITE actor_system_config

#include "caf/actor_system_config.hpp"

#include "caf/test/dsl.hpp"

#include <sstream>
#include <string>
#include <vector>

using namespace caf;

namespace {

using string_list = std::vector<std::string>;

struct config : actor_system_config {
  config() {
    opt_group{custom_options_, "?foo"}
      .add<std::string>("bar,b", "some string parameter");
  }
};

struct fixture {
  fixture() {
    ini << "[foo]\nbar=\"hello\"";
  }

  error parse(string_list args) {
    opts.clear();
    remainder.clear();
    config cfg;
    auto result = cfg.parse(std::move(args), ini);
    opts = content(cfg);
    remainder = cfg.remainder;
    return result;
  }

  std::stringstream ini;
  settings opts;
  std::vector<std::string> remainder;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(actor_system_config_tests, fixture)

CAF_TEST(parsing - without CLI arguments) {
  CAF_CHECK_EQUAL(parse({}), none);
  CAF_CHECK(remainder.empty());
  CAF_CHECK_EQUAL(get_or(opts, "foo.bar", ""), "hello");
}

CAF_TEST(parsing - without CLI remainder) {
  CAF_MESSAGE("CLI long name");
  CAF_CHECK_EQUAL(parse({"--foo.bar=test"}), none);
  CAF_CHECK(remainder.empty());
  CAF_CHECK_EQUAL(get_or(opts, "foo.bar", ""), "test");
  CAF_MESSAGE("CLI abbreviated long name");
  CAF_CHECK_EQUAL(parse({"--bar=test"}), none);
  CAF_CHECK(remainder.empty());
  CAF_CHECK_EQUAL(get_or(opts, "foo.bar", ""), "test");
  CAF_MESSAGE("CLI short name");
  CAF_CHECK_EQUAL(parse({"-b", "test"}), none);
  CAF_CHECK(remainder.empty());
  CAF_CHECK_EQUAL(get_or(opts, "foo.bar", ""), "test");
  CAF_MESSAGE("CLI short name without whitespace");
  CAF_CHECK_EQUAL(parse({"-btest"}), none);
  CAF_CHECK(remainder.empty());
  CAF_CHECK_EQUAL(get_or(opts, "foo.bar", ""), "test");
}

CAF_TEST(parsing - with CLI remainder) {
  CAF_MESSAGE("valid remainder");
  CAF_CHECK_EQUAL(parse({"-b", "test", "hello", "world"}), none);
  CAF_CHECK_EQUAL(get_or(opts, "foo.bar", ""), "test");
  CAF_CHECK_EQUAL(remainder, string_list({"hello", "world"}));
  CAF_MESSAGE("invalid remainder");
  CAF_CHECK_NOT_EQUAL(parse({"-b", "test", "-abc"}), none);
}

CAF_TEST_FIXTURE_SCOPE_END()
