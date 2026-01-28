// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/type_id.hpp"

#include "caf/test/test.hpp"

#include "caf/error.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/system_messages.hpp"

using namespace caf;
using namespace std::literals;

namespace {

struct test_type_1 {
  int32_t value;
};

template <class Inspector>
bool inspect(Inspector& f, test_type_1& x) {
  return f.object(x).fields(f.field("value", x.value));
}

struct test_type_2 {
  std::string name;
};

template <class Inspector>
bool inspect(Inspector& f, test_type_2& x) {
  return f.object(x).fields(f.field("name", x.name));
}

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(type_id_test, caf::first_custom_type_id + 240, 10)

  CAF_ADD_TYPE_ID(type_id_test, (test_type_1))
  CAF_ADD_TYPE_ID(type_id_test, (test_type_2), "custom_test_type_2")

CAF_END_TYPE_ID_BLOCK(type_id_test)

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::type_id_test>();
}

TEST("query_type_name returns type names for valid type IDs") {
  SECTION("built-in types") {
    check_eq(query_type_name(type_id_v<bool>), "bool"sv);
    check_eq(query_type_name(type_id_v<int32_t>), "int32_t"sv);
    check_eq(query_type_name(type_id_v<std::string>), "std::string"sv);
  }
  SECTION("custom types") {
    check_eq(query_type_name(type_id_v<test_type_1>), "test_type_1"sv);
    check_eq(query_type_name(type_id_v<test_type_2>), "custom_test_type_2"sv);
  }
}

TEST("query_type_name returns empty string for invalid type IDs") {
  check_eq(query_type_name(invalid_type_id), ""sv);
  check_eq(query_type_name(static_cast<type_id_t>(65534)), ""sv);
  check_eq(query_type_name(static_cast<type_id_t>(10000)), ""sv);
}

TEST("query_type_id returns type IDs for valid type names") {
  SECTION("built-in types") {
    check_eq(query_type_id("bool"sv), type_id_v<bool>);
    check_eq(query_type_id("int32_t"sv), type_id_v<int32_t>);
    check_eq(query_type_id("std::string"sv), type_id_v<std::string>);
  }
  SECTION("custom types") {
    check_eq(query_type_id("test_type_1"sv), type_id_v<test_type_1>);
    check_eq(query_type_id("custom_test_type_2"sv), type_id_v<test_type_2>);
  }
}

TEST("query_type_id returns invalid_type_id for unknown type names") {
  check_eq(query_type_id("unknown_type"sv), invalid_type_id);
  check_eq(query_type_id("nonexistent::type"sv), invalid_type_id);
  check_eq(query_type_id("definitely_not_a_type_name_12345"sv),
           invalid_type_id);
}

TEST("is_system_message identifies system message types") {
  SECTION("system messages return true") {
    check(is_system_message(type_id_v<exit_msg>));
    check(is_system_message(type_id_v<down_msg>));
    check(is_system_message(type_id_v<error>));
  }
  SECTION("non-system messages return false") {
    check(!is_system_message(type_id_v<bool>));
    check(!is_system_message(type_id_v<int32_t>));
    check(!is_system_message(type_id_v<std::string>));
    check(!is_system_message(type_id_v<test_type_1>));
    check(!is_system_message(type_id_v<test_type_2>));
  }
  SECTION("invalid type IDs return false") {
    check(!is_system_message(invalid_type_id));
    check(!is_system_message(static_cast<type_id_t>(10000)));
  }
}

TEST("default_type_id_mapper forwards to query functions") {
  default_type_id_mapper mapper;
  SECTION("type ID to name mapping") {
    check_eq(mapper(type_id_v<bool>), "bool"sv);
    check_eq(mapper(type_id_v<test_type_1>), "test_type_1"sv);
    check_eq(mapper(invalid_type_id), ""sv);
  }
  SECTION("name to type ID mapping") {
    check_eq(mapper("bool"sv), type_id_v<bool>);
    check_eq(mapper("test_type_1"sv), type_id_v<test_type_1>);
    check_eq(mapper("unknown_type"sv), invalid_type_id);
  }
}

TEST("type_name_v provides compile-time type names") {
  check_eq(type_name_v<bool>, "bool"sv);
  check_eq(type_name_v<test_type_1>, "test_type_1"sv);
  check_eq(type_name_v<test_type_2>, "custom_test_type_2"sv);
}

TEST("type_name_by_id_v provides type names from type IDs") {
  check_eq(type_name_by_id_v<type_id_v<bool>>, "bool"sv);
  check_eq(type_name_by_id_v<type_id_v<test_type_1>>, "test_type_1"sv);
}

TEST("type_by_id_t resolves types from type IDs") {
  check(std::is_same_v<type_by_id_t<type_id_v<bool>>, bool>);
  check(std::is_same_v<type_by_id_t<type_id_v<test_type_1>>, test_type_1>);
  check(std::is_same_v<type_by_id_t<type_id_v<int32_t>>, int32_t>);
}

TEST("type_id_or_invalid returns type ID or invalid_type_id") {
  check_eq(type_id_or_invalid<bool>(), type_id_v<bool>);
  check_eq(type_id_or_invalid<test_type_1>(), type_id_v<test_type_1>);
  struct unregistered_type {};
  check_eq(type_id_or_invalid<unregistered_type>(), invalid_type_id);
}

TEST("type_name_or_anonymous returns type name or anonymous") {
  check_eq(type_name_or_anonymous<bool>(), "bool"sv);
  check_eq(type_name_or_anonymous<test_type_1>(), "test_type_1"sv);
  struct unregistered_type {};
  check_eq(type_name_or_anonymous<unregistered_type>(), "anonymous"sv);
}

TEST("has_type_id_v checks if type has registered ID") {
  check(has_type_id_v<bool>);
  check(has_type_id_v<test_type_1>);
  struct unregistered_type {};
  check(!has_type_id_v<unregistered_type>);
}

TEST("type ID constants have expected values") {
  check_eq(invalid_type_id, 65535u);
  check_eq(first_custom_type_id, 200u);
  check_ge(type_id_v<test_type_1>, first_custom_type_id);
}

TEST("type ID registration creates bidirectional mappings") {
  constexpr auto id = type_id_v<test_type_1>;
  constexpr auto name = type_name_v<test_type_1>;
  check_eq(query_type_id(name), id);
  check_eq(query_type_name(id), name);
  check_eq(type_name_by_id_v<type_id_v<test_type_1>>, name);
  check(std::is_same_v<type_by_id_t<type_id_v<test_type_1>>, test_type_1>);
}
