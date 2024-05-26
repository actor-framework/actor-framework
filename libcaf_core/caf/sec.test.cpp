// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/sec.hpp"

#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/json_reader.hpp"
#include "caf/json_writer.hpp"

#include <optional>

using namespace caf;
using namespace std::literals;

namespace {

TEST("system error codes are convertible to strings") {
  check_eq(to_string(sec::none), "none");
  check_eq(to_string(sec::invalid_argument), "invalid_argument");
  check_eq(to_string(sec::no_such_key), "no_such_key");
}

TEST("system error codes are convertible from strings") {
  auto to_sec = [](std::string_view str) -> std::optional<sec> {
    sec result;
    if (from_string(str, result))
      return result;
    return std::nullopt;
  };
  check_eq(to_sec("none"), sec::none);
  check_eq(to_sec("invalid_argument"), sec::invalid_argument);
  check_eq(to_sec("no_such_key"), sec::no_such_key);
}

TEST("system error codes are inspectable") {
  SECTION("human-readable representation") {
    json_writer writer;
    auto val = sec::no_such_key;
    check(inspect(writer, val));
    check_eq(writer.str(), R"_("no_such_key")_");
    json_reader reader;
    auto copy = sec::none;
    check(reader.load(writer.str()));
    check(inspect(reader, copy));
    check_eq(copy, val);
  }
  SECTION("binary representation") {
    byte_buffer buf;
    auto val = sec::no_such_key;
    binary_serializer sink{buf};
    check(inspect(sink, val));
    auto copy = sec::none;
    binary_deserializer source{buf};
    check(inspect(source, copy));
    check_eq(copy, val);
  }
}

} // namespace
