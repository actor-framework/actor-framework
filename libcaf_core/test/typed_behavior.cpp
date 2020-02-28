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

#define CAF_SUITE typed_behavior

#include "caf/typed_behavior.hpp"

#include "caf/test/dsl.hpp"

#include <cstdint>
#include <string>

#include "caf/typed_actor.hpp"

using namespace caf;

CAF_TEST(make_typed_behavior automatically deduces its types) {
  using handle
    = typed_actor<reacts_to<std::string>, replies_to<int32_t>::with<int32_t>,
                  replies_to<double>::with<double>>;
  auto bhvr = make_typed_behavior([](const std::string&) {},
                                  [](int32_t x) { return x; },
                                  [](double x) { return x; });
  static_assert(std::is_same<handle::behavior_type, decltype(bhvr)>::value, "");
}
