// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
  static_assert(std::is_same<handle::behavior_type, decltype(bhvr)>::value);
}
