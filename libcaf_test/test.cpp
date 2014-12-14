#include <cstdlib>

#include "caf/all.hpp"
#include "caf/test/unit_test.hpp"

int main(int argc, char* argv[]) {
  using namespace caf;
  // Default values.
  auto colorize = true;
  std::string log_file;
  int verbosity_console = 3;
  int verbosity_file = 3;
  int max_runtime = 10;
  std::regex suites{".*"};
  std::regex not_suites;
  std::regex tests{".*"};
  std::regex not_tests;
  // Our simple command line parser.
  message_handler parser{
    on("-n") >> [&] {
      colorize = false;
    },
    on("-l", arg_match) >> [&](const std::string& file_name) {
      log_file = file_name;
    },
    on("-v", arg_match) >> [&](const std::string& n) {
      verbosity_console = static_cast<int>(std::strtol(n.c_str(), nullptr, 10));
    },
    on("-V", arg_match) >> [&](const std::string& n) {
      verbosity_file = static_cast<int>(std::strtol(n.c_str(), nullptr, 10));
    },
    on("-r", arg_match) >> [&](const std::string& n) {
      max_runtime = static_cast<int>(std::strtol(n.c_str(), nullptr, 10));
    },
    on("-s", arg_match) >> [&](const std::string& str) {
      suites = str;
    },
    on("-S", arg_match) >> [&](const std::string& str) {
      not_suites = str;
    },
    on("-t", arg_match) >> [&](const std::string& str) {
      tests = str;
    },
    on("-T", arg_match) >> [&](const std::string& str) {
      not_tests = str;
    },
    others() >> [&] {
      std::cerr << "invalid command line argument" << std::endl;
      std::exit(1);
    }
  };
  auto is_opt_char = [](char c) {
    switch (c) {
      default:
        return false;
      case 'n':
      case 'l':
      case 'v':
      case 'V':
      case 'r':
      case 's':
      case 'S':
      case 't':
      case 'T':
        return true;
    };
  };
  for (int i = 1; i < argc; ++i) {
    message_builder builder;
    std::string arg{argv[i]};
    if (arg.empty() || arg[0] != '-') {
      std::cerr << "invalid option: " << arg << std::endl;
      return 1;
    }
    builder.append(std::move(arg));
    if (i + 1 < argc) {
      auto next = argv[i + 1];
      if (*next == '\0' || next[0] != '-' || ! is_opt_char(next[1])) {
        builder.append(std::string{next});
        ++i;
      }
    }
    builder.apply(parser);
  }
  auto result = caf::test::engine::run(colorize, log_file, verbosity_console,
                                       verbosity_file, max_runtime, suites,
                                       not_suites, tests, not_tests);
  return result ? 0 : 1;
}
