// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/test_export.hpp"

#include <string>
#include <string_view>
#include <type_traits>

namespace caf::test {

/// Identifies a code block in a test definition.
enum class block_type {
  /// Identifies a TEST block.
  test,
  /// Identifies a SECTION block in a TEST.
  section,
  /// Identifies a parametrized BDD-style OUTLINE block.
  outline,
  /// Identifies a BDD-style SCENARIO block.
  scenario,
  /// Identifies a BDD-style GIVEN block.
  given,
  /// Identifies a BDD-style AND_GIVEN block.
  and_given,
  /// Identifies a BDD-style WHEN block.
  when,
  /// Identifies a BDD-style AND_WHEN block.
  and_when,
  /// Identifies a BDD-style THEN block.
  then,
  /// Identifies a BDD-style AND_THEN block.
  and_then,
  /// Identifies a BDD-style BUT block.
  but,
};

/// Checks whether `type` is an extension type, i.e., AND_GIVEN, AND_WHEN,
/// AND_THEN, or BUT.
/// @relates block_type
constexpr bool is_extension(block_type type) noexcept {
  switch (type) {
    default:
      return false;
    case block_type::and_given:
    case block_type::and_when:
    case block_type::and_then:
    case block_type::but:
      return true;
  }
}

/// @relates block_type
constexpr std::string_view macro_name(block_type type) noexcept {
  switch (type) {
    case block_type::test:
      return "TEST";
    case block_type::section:
      return "SECTION";
    case block_type::outline:
      return "OUTLINE";
    case block_type::scenario:
      return "SCENARIO";
    case block_type::given:
      return "GIVEN";
    case block_type::and_given:
      return "AND_GIVEN";
    case block_type::when:
      return "WHEN";
    case block_type::and_when:
      return "AND_WHEN";
    case block_type::then:
      return "THEN";
    case block_type::and_then:
      return "AND_THEN";
    case block_type::but:
      return "BUT";
  }
  return "???";
}

/// @relates block_type
constexpr std::string_view as_prefix(block_type type) noexcept {
  switch (type) {
    case block_type::test:
      return "Test";
    case block_type::section:
      return "Section";
    case block_type::outline:
      return "Outline";
    case block_type::scenario:
      return "Scenario";
    case block_type::given:
      return "Given";
    case block_type::when:
      return "When";
    case block_type::then:
      return "Then";
    case block_type::and_given:
    case block_type::and_when:
    case block_type::and_then:
      return "And";
    case block_type::but:
      return "But";
  }
  return "???";
}

/// @relates block_type
CAF_TEST_EXPORT std::string to_string(block_type);

/// @relates block_type
CAF_TEST_EXPORT bool from_string(std::string_view, block_type&);

/// @relates block_type
CAF_TEST_EXPORT bool from_integer(std::underlying_type_t<block_type>,
                                  block_type&);

} // namespace caf::test
