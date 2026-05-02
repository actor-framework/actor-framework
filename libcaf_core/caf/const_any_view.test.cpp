// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/const_any_view.hpp"

#include "caf/test/scenario.hpp"

#include "caf/detail/any_ref_impl.hpp"
#include "caf/type_id.hpp"

#include <utility>

using namespace caf;

SCENARIO("const_any_view may reference any announced type") {
  GIVEN("a const_any_view referencing a value of type int32_t") {
    int32_t ival = 7;
    detail::any_ref_impl ival_ref{ival};
    const_any_view ival_view{std::as_const(ival_ref)};
    WHEN("accessing the value through const_any_view") {
      THEN("type_id() and get() provide manual type check and access") {
        if (check_eq(ival_view.type_id(), type_id_v<int32_t>)) {
          check_eq(ival_view.get<int32_t>(), ival);
        }
      }
      AND_THEN("get_if() provides type-checked access") {
        check_eq(ival_view.get_if<double>(), nullptr);
        check_eq(ival_view.get_if<int32_t>(), std::addressof(ival));
      }
    }
  }
}
