// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE typed_behavior

#include "caf/typed_behavior.hpp"

#include "core-test.hpp"

#include <cstdint>
#include <string>

#include "caf/typed_actor.hpp"

using namespace caf;

CAF_TEST(make_typed_behavior automatically deduces its types) {
  using handle = typed_actor<result<void>(std::string),
                             result<int32_t>(int32_t), result<double>(double)>;
  auto bhvr = make_typed_behavior([](const std::string&) {},
                                  [](int32_t x) { return x; },
                                  [](double x) { return x; });
  static_assert(std::is_same<handle::behavior_type, decltype(bhvr)>::value);
}
