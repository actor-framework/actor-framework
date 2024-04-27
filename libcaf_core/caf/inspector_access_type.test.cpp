// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/inspector_access_type.hpp"

#include "caf/test/test.hpp"

#include "caf/action.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/inspector_access_base.hpp"

#include <vector>

using namespace caf;
using namespace std::literals;

namespace {

TEST("inspect_access_type") {
  check(
    std::is_same_v<decltype(inspect_access_type<caf::binary_serializer, int>()),
                   inspector_access_type::builtin>);
  check(std::is_same_v<decltype(inspect_access_type<caf::binary_serializer,
                                                    std::array<int, 5>>()),
                       inspector_access_type::tuple>);
  check(std::is_same_v<decltype(inspect_access_type<caf::binary_serializer,
                                                    std::optional<int>>()),
                       inspector_access_type::specialization>);
  check(
    std::is_same_v<
      decltype(inspect_access_type<caf::binary_serializer, std::vector<int>>()),
      inspector_access_type::list>);
  check(std::is_same_v<decltype(inspect_access_type<caf::binary_serializer,
                                                    std::map<int, int>>()),
                       inspector_access_type::map>);
  check(std::is_same_v<
        decltype(inspect_access_type<caf::binary_serializer, int*>()),
        inspector_access_type::none>);
  check(std::is_same_v<
        decltype(inspect_access_type<caf::binary_serializer, int[]>()),
        inspector_access_type::tuple>);
  check(std::is_same_v<
        decltype(inspect_access_type<caf::binary_serializer, std::string>()),
        inspector_access_type::builtin>);
  check(std::is_same_v<
        decltype(inspect_access_type<caf::binary_serializer, int const[]>()),
        inspector_access_type::tuple>);
  check(
    std::is_same_v<
      decltype(inspect_access_type<caf::binary_serializer, std::false_type>()),
      inspector_access_type::empty>);
  check(std::is_same_v<
        decltype(inspect_access_type<caf::binary_serializer, error>()),
        inspector_access_type::inspect>);
  check(std::is_same_v<
        decltype(inspect_access_type<caf::binary_serializer, action>()),
        inspector_access_type::unsafe>);
}

} // namespace
