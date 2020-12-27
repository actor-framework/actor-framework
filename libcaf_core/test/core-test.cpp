#define CAF_TEST_NO_MAIN

#include "caf/test/unit_test_impl.hpp"

#include "core-test.hpp"

std::string to_string(level lvl) {
  switch (lvl) {
    case level::all:
      return "all";
    case level::trace:
      return "trace";
    case level::debug:
      return "debug";
    case level::warning:
      return "warning";
    case level::error:
      return "error";
    default:
      return "???";
  }
}

bool from_string(caf::string_view str, level& lvl) {
  auto set = [&](level value) {
    lvl = value;
    return true;
  };
  if (str == "all")
    return set(level::all);
  else if (str == "trace")
    return set(level::trace);
  else if (str == "debug")
    return set(level::debug);
  else if (str == "warning")
    return set(level::warning);
  else if (str == "error")
    return set(level::error);
  else
    return false;
}

bool from_integer(uint8_t val, level& lvl) {
  if (val < 5) {
    lvl = static_cast<level>(val);
    return true;
  } else {
    return false;
  }
}

int main(int argc, char** argv) {
  using namespace caf;
  init_global_meta_objects<id_block::core_test>();
  core::init_global_meta_objects();
  return test::main(argc, argv);
}
