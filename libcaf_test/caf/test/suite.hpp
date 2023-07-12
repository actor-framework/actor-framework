// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/pp.hpp"
#include "caf/unit.hpp"

#include <string_view>

[[maybe_unused]] constexpr caf::unit_t caf_test_suite_name = caf::unit;

struct caf_test_case_auto_fixture {};

#define SUITE(name)                                                            \
  namespace {                                                                  \
  static_assert(                                                               \
    std::is_same_v<std::decay_t<decltype(caf_test_suite_name)>, caf::unit_t>,  \
    "only one SUITE per translation unit is supported");                       \
  constexpr std::string_view caf_test_suite_name = name;                       \
  }                                                                            \
  namespace

#define WITH_FIXTURE(name)                                                     \
  namespace CAF_PP_UNIFYN(caf_fixture_) {                                      \
  using caf_test_case_auto_fixture = name;                                     \
  }                                                                            \
  namespace CAF_PP_UNIFYN(caf_fixture_)
