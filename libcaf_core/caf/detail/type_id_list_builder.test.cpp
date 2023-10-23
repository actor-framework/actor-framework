// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/type_id_list_builder.hpp"

#include "caf/test/outline.hpp"
#include "caf/test/scenario.hpp"

#include "caf/type_id.hpp"

using namespace caf;

namespace {

using builder_t = detail::type_id_list_builder;

SCENARIO("a default-constructed type_id_list_builder is empty") {
  GIVEN("a default-constructed type_id_list_builder") {
    detail::type_id_list_builder builder;
    WHEN("we ask for its size") {
      THEN("we get 0") {
        check_eq(builder.size(), 0u);
      }
    }
    WHEN("we ask for its capacity") {
      THEN("we get 0") {
        check_eq(builder.capacity(), 0u);
      }
    }
    WHEN("we call copy_to_list") {
      auto list = builder.copy_to_list();
      THEN("the returned list is empty") {
        check_eq(list.size(), 0u);
      }
    }
    WHEN("we call move_to_list") {
      auto list = builder.move_to_list();
      THEN("the returned list is empty") {
        check_eq(list.size(), 0u);
      }
    }
  }
}

OUTLINE("push_back adds elements to the end of the list") {
  GIVEN("an empty type_id_list_builder") {
    detail::type_id_list_builder builder;
    WHEN("calling push_back consecutively with values <values>") {
      auto values = block_parameters<std::vector<type_id_t>>();
      for (auto value : values) {
        builder.push_back(value);
      }
      THEN("the list contains the elements") {
        if (check_eq(builder.size(), values.size())) {
          for (size_t index = 0; index < values.size(); ++index) {
            check_eq(builder[index], values[index]);
          }
        }
      }
      AND_THEN("the builder has capacity <capacity>") {
        auto capacity = block_parameters<size_t>();
        check_eq(builder.capacity(), capacity);
      }
      AND_THEN("calling copy_to_list copies the elements to a type_id_list") {
        auto list = builder.copy_to_list();
        if (check_eq(builder.size(), values.size())) {
          for (size_t index = 0; index < values.size(); ++index) {
            check_eq(list[index], values[index]);
          }
        }
      }
      AND_THEN("calling move_to_list moves the elements to a type_id_list") {
        auto list = builder.move_to_list();
        if (check_eq(builder.size(), values.size())) {
          for (size_t index = 0; index < values.size(); ++index) {
            check_eq(list[index], values[index]);
          }
        }
      }
      AND_THEN("calling clear will restore the builder to its default state") {
        builder.clear();
        check_eq(builder.size(), 0u);
        check_eq(builder.capacity(), 0u);
      }
    }
  }
  // Note:  6 = type_id_v<int8_t>
  //        3 = type_id_v<int16_t>
  //        4 = type_id_v<int32_t>
  //        5 = type_id_v<int64_t>
  //       11 = type_id_v<uint8_t>
  //        8 = type_id_v<uint16_t>
  //        9 = type_id_v<uint32_t>
  //       10 = type_id_v<uint64_t>
  EXAMPLES = R"(
    | values                     | capacity |
    | [6]                        |        8 |
    | [6, 3]                     |        8 |
    | [6, 3, 4]                  |        8 |
    | [6, 3, 4, 5]               |        8 |
    | [6, 3, 4, 5, 11]           |        8 |
    | [6, 3, 4, 5, 11, 8]        |        8 |
    | [6, 3, 4, 5, 11, 8, 9]     |        8 |
    | [6, 3, 4, 5, 11, 8, 9, 10] |       16 |
  )";
}

OUTLINE("passing an size hint to the builder pre-allocates memory") {
  GIVEN("a size hint <hint>") {
    auto hint = block_parameters<size_t>();
    WHEN("constructing a builder with the size hint") {
      detail::type_id_list_builder builder{hint};
      THEN("the capacity is <capacity>") {
        auto capacity = block_parameters<size_t>();
        check_eq(builder.capacity(), capacity);
      }
    }
  }
  // Note: the builder must have space for at least one extra element: the size
  //       dummy. Hence, it will jump to the next block size if the hint is
  //       exactly at the end of a block.
  EXAMPLES = R"(
    | hint | capacity |
    |    0 |        0 |
    |    1 |        8 |
    |    2 |        8 |
    |    3 |        8 |
    |    4 |        8 |
    |    5 |        8 |
    |    6 |        8 |
    |    7 |        8 |
    |    8 |       16 |
    |    9 |       16 |
    |   15 |       16 |
    |   16 |       24 |
    |   17 |       24 |
    |   23 |       24 |
    |   24 |       32 |
  )";
}

} // namespace
