// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/meta/handler.hpp"

#include "caf/test/test.hpp"

using namespace caf;

TEST("handlers are convertible to strings") {
  check_eq(to_string(meta::handler{
             make_type_id_list<>(),
             make_type_id_list<>(),
           }),
           "() -> ()");
  check_eq(to_string(meta::handler{
             make_type_id_list<int32_t>(),
             make_type_id_list<>(),
           }),
           "(int32_t) -> ()");
  check_eq(to_string(meta::handler{
             make_type_id_list<>(),
             make_type_id_list<int32_t>(),
           }),
           "() -> (int32_t)");
  check_eq(to_string(meta::handler{
             make_type_id_list<int32_t, int16_t, int8_t>(),
             make_type_id_list<int8_t, int16_t, int32_t>(),
           }),
           "(int32_t, int16_t, int8_t) -> (int8_t, int16_t, int32_t)");
}
