/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_TEST_UNIT_TEST_IMPL_HPP
#define CAF_TEST_UNIT_TEST_IMPL_HPP

#include <regex>
#include <thread>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <condition_variable>

#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"
#include "caf/string_algorithms.hpp"

#include "caf/test/unit_test.hpp"

namespace caf {
namespace test {

class watchdog {
public:
   static void start(int secs);
   static void stop();

private:
  watchdog(int secs) {
    thread_ = std::thread{[=] {
      auto tp =
        std::chrono::high_resolution_clock::now() + std::chrono::seconds(secs);
        std::unique_lock<std::mutex> guard{mtx_};
      while (! canceled_
             && cv_.wait_until(guard, tp) != std::cv_status::timeout) {
        // spin
      }
      if (! canceled_) {
        logger::instance().error()
          << "WATCHDOG: unit test did not finish within "
          << secs << "s, abort\n";
        abort();
      }
    }};
  }
  ~watchdog() {
    { // lifetime scope of guard
      std::lock_guard<std::mutex> guard{mtx_};
      canceled_ = true;
      cv_.notify_all();
    }
    thread_.join();
  }

  volatile bool canceled_ = false;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::thread thread_;
};

namespace { watchdog* s_watchdog; }

void watchdog::start(int secs) {
  s_watchdog = new watchdog(secs);
}

void watchdog::stop() {
  delete s_watchdog;
}

test::test(std::string test_name)
    : expected_failures_(0),
      name_(std::move(test_name)),
      good_(0),
      bad_(0) {
  // nop
}

test::~test() {
  // nop
}

size_t test::expected_failures() const {
  return expected_failures_;
}

void test::pass(std::string msg) {
  ++good_;
  ::caf::test::logger::instance().massive() << "  " << msg << '\n';
}

void test::fail(std::string msg, bool expected) {
  ++bad_;
  ::caf::test::logger::instance().massive() << "  " << msg << '\n';
  if (expected) {
    ++expected_failures_;
  }
}

const std::string& test::name() const {
  return name_;
}

namespace detail {

require_error::require_error(const std::string& msg) : std::logic_error(msg) {
  // nop
}

require_error::~require_error() noexcept {
  // nop
}

const char* fill(size_t line) {
  if (line < 10) {
    return "    ";
  } else if (line < 100) {
    return "   ";
  } else if (line < 1000) {
    return "  ";
  } else {
    return " ";
  }
}

} // namespace detail

logger::stream::stream(logger& l, level lvl) : logger_(l), level_(lvl) {
  // nop
}

logger::stream::stream(stream&& other)
    : logger_(other.logger_)
    , level_(other.level_) {
  // ostringstream does have swap since C++11... but not in GCC 4.8.4
  buf_.str(other.buf_.str());
}

logger::stream& logger::stream::operator<<(const char& c) {
  buf_ << c;
  if (c == '\n') {
    flush();
  }
  return *this;
}

logger::stream& logger::stream::operator<<(const char* cstr) {
  if (*cstr == '\0') {
    return *this;
  }
  buf_ << cstr;
  if (cstr[std::strlen(cstr) - 1] == '\n') {
    flush();
  }
  return *this;
}

logger::stream& logger::stream::operator<<(const std::string& str) {
  if (str.empty()) {
    return *this;
  }
  buf_ << str;
  if (str.back() == '\n') {
    flush();
  }
  return *this;
}

void logger::stream::flush() {
  logger_.log(level_, buf_.str());
  buf_.str("");
}


bool logger::init(int lvl_cons, int lvl_file, const std::string& logfile) {
  instance().level_console_ = static_cast<level>(lvl_cons);
  instance().level_file_ = static_cast<level>(lvl_file);
  if (! logfile.empty()) {
    instance().file_.open(logfile, std::ofstream::out | std::ofstream::app);
    return !! instance().file_;
  }
  return true;
}

logger& logger::instance() {
  static logger l;
  return l;
}

logger::stream logger::error() {
  return stream{*this, level::error};
}

logger::stream logger::info() {
  return stream{*this, level::info};
}

logger::stream logger::verbose() {
  return stream{*this, level::verbose};
}

logger::stream logger::massive() {
  return stream{*this, level::massive};
}

logger::logger()
    : level_console_(level::error),
      level_file_(level::error),
      console_(std::cerr) {
  // nop
}

void engine::args(int argc, char** argv) {
  instance().argc_ = argc;
  instance().argv_ = argv;
}

int engine::argc() {
  return instance().argc_;
}

char** engine::argv() {
  return instance().argv_;
}

void engine::path(char* argv) {
  instance().path_ = argv;
}

char* engine::path() {
  return instance().path_;
}

int engine::max_runtime() {
  return instance().max_runtime_;
}

void engine::max_runtime(int value) {
  instance().max_runtime_ = value;
}

void engine::add(const char* cstr_name, std::unique_ptr<test> ptr) {
  std::string name = cstr_name ? cstr_name : "";
  auto& suite = instance().suites_[name];
  for (auto& x : suite) {
    if (x->name() == ptr->name()) {
      std::cerr << "duplicate test name: " << ptr->name() << std::endl;
      std::abort();
    }
  }
  suite.emplace_back(std::move(ptr));
}

bool engine::run(bool colorize,
                 const std::string& log_file,
                 int verbosity_console,
                 int verbosity_file,
                 const std::string& suites_str,
                 const std::string& not_suites_str,
                 const std::string& tests_str,
                 const std::string& not_tests_str) {
  if (not_suites_str == "*" || not_tests_str == "*") {
    // nothing to do
    return true;
  }
  if (! colorize) {
    for (size_t i = 0; i <= static_cast<size_t>(white); ++i) {
      for (size_t j = 0; j <= static_cast<size_t>(bold); ++j) {
        instance().colors_[i][j] = "";
      }
    }
  }
  if (! logger::init(verbosity_console, verbosity_file, log_file)) {
    return false;
  }
  auto& log = logger::instance();
  std::chrono::microseconds runtime{0};
  size_t total_suites = 0;
  size_t total_tests = 0;
  size_t total_good = 0;
  size_t total_bad = 0;
  size_t total_bad_expected = 0;
  auto bar = '+' + std::string(70, '-') + '+';
  auto failed_require = false;
# if (! defined(__clang__) && defined(__GNUC__)                                 \
      && __GNUC__ == 4 && __GNUC_MINOR__ < 9)                                  \
     || (defined(__clang__) && ! defined(_LIBCPP_VERSION))
  // regex implementation is broken prior to 4.9
  using strvec = std::vector<std::string>;
  auto from_psv = [](const std::string& psv) -> strvec {
    // psv == pipe-separated-values
    strvec result;
    if (psv != ".*") {
      split(result, psv, "|", token_compress_on);
      std::sort(result.begin(), result.end());
    }
    return result;
  };
  auto suites = from_psv(suites_str);
  auto not_suites = from_psv(not_suites_str);
  auto tests = from_psv(tests_str);
  auto not_tests = from_psv(not_tests_str);
  auto enabled = [](const strvec& whitelist,
                    const strvec& blacklist,
                    const std::string& x) {
    // an empty whitelist means original input was ".*", i.e., enable all
    return ! std::binary_search(blacklist.begin(), blacklist.end(), x)
           && (whitelist.empty()
               || std::binary_search(whitelist.begin(), whitelist.end(), x));
  };
# else
  std::regex suites{suites_str};
  std::regex tests{tests_str};
  std::regex not_suites;
  std::regex not_tests;
  // a default constructored regex matches is not equal to an "empty" regex
  if (! not_suites_str.empty()) {
    not_suites.assign(not_suites_str);
  }
  if (! not_tests_str.empty()) {
    not_tests.assign(not_tests_str);
  }
  auto enabled = [](const std::regex& whitelist,
                    const std::regex& blacklist,
                    const std::string& x) {
    // an empty whitelist means original input was "*", i.e., enable all
    return std::regex_search(x, whitelist)
           && ! std::regex_search(x, blacklist);
  };
# endif
  for (auto& p : instance().suites_) {
    if (! enabled(suites, not_suites, p.first)) {
      continue;
    }
    auto suite_name = p.first.empty() ? "<unnamed>" : p.first;
    auto pad = std::string((bar.size() - suite_name.size()) / 2, ' ');
    bool displayed_header = false;
    size_t tests_ran = 0;
    for (auto& t : p.second) {
      if (! enabled(tests, not_tests, t->name())) {
        continue;
      }
      instance().current_test_ = t.get();
      ++tests_ran;
      if (! displayed_header) {
        log.verbose() << color(yellow) << bar << '\n' << pad << suite_name
                      << '\n' << bar << color(reset) << "\n\n";
        displayed_header = true;
      }
      log.verbose() << color(yellow) << "- " << color(reset) << t->name()
                    << '\n';
      auto start = std::chrono::high_resolution_clock::now();
      watchdog::start(max_runtime());
      try {
        t->run();
      } catch (const detail::require_error&) {
        failed_require = true;
      }
      watchdog::stop();
      auto stop = std::chrono::high_resolution_clock::now();
      auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
      runtime += elapsed;
      ++total_tests;
      size_t good = t->good();
      size_t bad = t->bad();
      if (failed_require) {
        log.error() << color(red) << "     REQUIRED" << color(reset) << '\n'
                    << "     " << color(blue) << last_check_file()
                    << color(yellow) << ":" << color(cyan) << last_check_line()
                    << color(reset) << detail::fill(last_check_line())
                    << "had last successful check" << '\n';
      }
      total_good += good;
      total_bad += bad;
      total_bad_expected += t->expected_failures();
      log.verbose() << color(yellow) << "  -> " << color(cyan) << good + bad
                    << color(reset) << " check" << (good + bad > 1 ? "s " : " ")
                    << "took " << color(cyan) << render(elapsed)
                    << color(reset) << '\n';
      if (bad > 0) {
        log.verbose() << " (" << color(green) << good << color(reset) << '/'
                      << color(red) << bad << color(reset) << ")" << '\n';
      } else {
        log.verbose() << '\n';
      }
      if (failed_require) {
        break;
      }
    }
    // only count suites which have executed one or more tests
    if (tests_ran > 0) {
      ++total_suites;
    }
    if (displayed_header) {
      log.verbose() << '\n';
    }
    if (failed_require) {
      break;
    }
  }
  unsigned percent_good = 100;
  if (total_bad > 0) {
    auto tmp = (100000.0 * static_cast<double>(total_good))
               / static_cast<double>(total_good + total_bad
                                     - total_bad_expected);
    percent_good = static_cast<unsigned>(tmp / 1000.0);
  }
  auto title = std::string{"summary"};
  auto pad = std::string((bar.size() - title.size()) / 2, ' ');
  auto indent = std::string(24, ' ');
  log.info() << color(cyan) << bar << '\n' << pad << title << '\n' << bar
             << color(reset) << "\n\n" << indent << "suites:  " << color(yellow)
             << total_suites << color(reset) << '\n' << indent
             << "tests:   " << color(yellow) << total_tests << color(reset)
             << '\n' << indent << "checks:  " << color(yellow)
             << total_good + total_bad << color(reset);
  if (total_bad > 0) {
    log.info() << " (" << color(green) << total_good << color(reset) << '/'
               << color(red) << total_bad << color(reset) << ")";
    if (total_bad_expected > 0) {
      log.info()
        << ' ' << color(cyan) << total_bad_expected << color(reset)
        << " failures expected";
    }
  }
  log.info() << '\n' << indent << "time:    " << color(yellow)
             << render(runtime) << '\n' << color(reset) << indent
             << "success: "
             << (total_bad == total_bad_expected ? color(green) : color(red))
             << percent_good << "%" << color(reset) << "\n\n" << color(cyan)
             << bar << color(reset) << '\n';
  return total_bad == total_bad_expected;
}

const char* engine::color(color_value v, color_face t) {
  return instance().colors_[static_cast<size_t>(v)][static_cast<size_t>(t)];
}

const char* engine::last_check_file() {
  return instance().check_file_;
}

void engine::last_check_file(const char* file) {
  instance().check_file_ = file;
}

size_t engine::last_check_line() {
  return instance().check_line_;
}

void engine::last_check_line(size_t line) {
  instance().check_line_ = line;
}

test* engine::current_test() {
  return instance().current_test_;
}

std::vector<std::string> engine::available_suites() {
  std::vector<std::string> result;
  for (auto& kvp : instance().suites_) {
    result.push_back(kvp.first);
  }
  return result;
}

engine& engine::instance() {
  static engine e;
  return e;
}

std::string engine::render(std::chrono::microseconds t) {
  return t.count() > 1000000
    ? (std::to_string(t.count() / 1000000) + '.'
       + std::to_string((t.count() % 1000000) / 10000) + " s")
    : t.count() > 1000
      ? (std::to_string(t.count() / 1000) + " ms")
      : (std::to_string(t.count()) + " us");
}

int main(int argc, char** argv) {
  // set path of executable
  engine::path(argv[0]);
  // default values.
  int verbosity_console = 3;
  int verbosity_file = 3;
  int max_runtime = 0;
  std::string log_file;
  std::string suites = ".*";
  std::string not_suites;
  std::string tests = ".*";
  std::string not_tests;
  // use all arguments after '--' for the test engine.
  std::string delimiter = "--";
  auto divider = argc;
  auto cli_argv = argv + 1;
  for (auto i = 1; i < argc; ++i) {
    if (delimiter == argv[i]) {
      divider = i;
      break;
    }
  }
  // our simple command line parser.
  auto res = message_builder(cli_argv, cli_argv + divider - 1).extract_opts({
    {"no-colors,n", "disable coloring"},
    {"log-file,l", "set output file", log_file},
    {"console-verbosity,v", "set verbosity level of console (1-5)",
     verbosity_console},
    {"file-verbosity,V", "set verbosity level of file output (1-5)",
     verbosity_file},
    {"max-runtime,r", "set maximum runtime in seconds", max_runtime},
    {"suites,s",
     "define what suites to run, either * or a comma-separated list", suites},
    {"not-suites,S", "exclude suites", not_suites},
    {"tests,t", "set tests", tests},
    {"not-tests,T", "exclude tests", not_tests},
    {"available-suites,a", "print available suites"}
  });
  if (res.opts.count("help") > 0) {
    std::cout << res.helptext << std::endl;
    return 0;
  }
  if (res.opts.count("available-suites") > 0) {
    std::cout << "available suites:" << std::endl;
    for (auto& s : engine::available_suites()) {
      std::cout << "  - " << s << std::endl;
    }
    return 0;
  }
  if (! res.remainder.empty()) {
    std::cerr << "*** invalid command line options" << std::endl
              << res.helptext << std::endl;
    return 1;
  }
  auto colorize = res.opts.count("no-colors") == 0;
  if (divider < argc)
    engine::args(argc - divider - 1, argv + divider + 1);
  else
    engine::args(0, argv);
  if (res.opts.count("max-runtime") > 0)
    engine::max_runtime(max_runtime);
  auto result = engine::run(colorize, log_file, verbosity_console,
                            verbosity_file, suites,
                            not_suites, tests, not_tests);
  return result ? 0 : 1;
}

namespace detail {

expr::expr(test* parent, const char* filename, size_t lineno,
           bool should_fail, const char* expression)
    : test_{parent},
      filename_{filename},
      line_{lineno},
      should_fail_{should_fail},
      expr_{expression} {
  assert(test_ != nullptr);
}

} // namespace detail
} // namespace test
} // namespace caf

#ifndef CAF_TEST_NO_MAIN
int main(int argc, char** argv) {
  try {
    return caf::test::main(argc, argv);
  } catch (std::exception& e) {
    std::cerr << "exception " << typeid(e).name() << ": "
              << e.what() << std::endl;
  }
  return 1;
}
#endif

#endif // CAF_TEST_UNIT_TEST_IMPL_HPP
