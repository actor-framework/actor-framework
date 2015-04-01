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

#include <thread>
#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <condition_variable>

#include "caf/shutdown.hpp"
#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/await_all_actors_done.hpp"

namespace caf {
namespace test {

class watchdog {
 public:
   static void start();
   static void stop();

 private:
  watchdog() {
    m_thread = std::thread([&] {
      auto tp =
        std::chrono::high_resolution_clock::now() + std::chrono::seconds(10);
        std::unique_lock<std::mutex> guard{m_mtx};
      while (!m_canceled
             && m_cv.wait_until(guard, tp) != std::cv_status::timeout) {
        // spin
      }
      if (!m_canceled) {
        logger::instance().error()
          << "WATCHDOG: unit test did finish within 10s, abort\n";
        abort();
      }
    });
  }
  ~watchdog() {
    { // lifetime scope of guard
      std::lock_guard<std::mutex> guard{m_mtx};
      m_canceled = true;
      m_cv.notify_all();
    }
    m_thread.join();
  }

  volatile bool m_canceled = false;
  std::mutex m_mtx;
  std::condition_variable m_cv;
  std::thread m_thread;
};

namespace { watchdog* s_watchdog; }

void watchdog::start() {
  s_watchdog = new watchdog();
}

void watchdog::stop() {
  delete s_watchdog;
}

test::test(std::string test_name)
    : m_expected_failures(0),
      m_name(std::move(test_name)),
      m_good(0),
      m_bad(0) {
  // nop
}

test::~test() {
  // nop
}

size_t test::expected_failures() const {
  return m_expected_failures;
}

void test::pass(std::string msg) {
  ++m_good;
  ::caf::test::logger::instance().massive() << "  " << msg << '\n';
}

void test::fail(std::string msg, bool expected) {
  ++m_bad;
  ::caf::test::logger::instance().massive() << "  " << msg << '\n';
  if (expected) {
    ++m_expected_failures;
  }
}

const std::string& test::name() const {
  return m_name;
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

logger::stream::stream(logger& l, level lvl) : m_logger(l), m_level(lvl) {
  // nop
}

logger::stream::stream(stream&& other)
    : m_logger(other.m_logger)
    , m_level(other.m_level) {
  // ostringstream does have swap since C++11... but not in GCC 4.7
  m_buf.str(other.m_buf.str());
}

logger::stream& logger::stream::operator<<(const char& c) {
  m_buf << c;
  if (c == '\n') {
    flush();
  }
  return *this;
}

logger::stream& logger::stream::operator<<(const char* cstr) {
  if (*cstr == '\0') {
    return *this;
  }
  m_buf << cstr;
  if (cstr[strlen(cstr) - 1] == '\n') {
    flush();
  }
  return *this;
}

logger::stream& logger::stream::operator<<(const std::string& str) {
  if (str.empty()) {
    return *this;
  }
  m_buf << str;
  if (str.back() == '\n') {
    flush();
  }
  return *this;
}

void logger::stream::flush() {
  m_logger.log(m_level, m_buf.str());
  m_buf.str("");
}


bool logger::init(int lvl_cons, int lvl_file, const std::string& logfile) {
  instance().m_level_console = static_cast<level>(lvl_cons);
  instance().m_level_file = static_cast<level>(lvl_file);
  if (!logfile.empty()) {
    instance().m_file.open(logfile, std::ofstream::out | std::ofstream::app);
    return !!instance().m_file;
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

logger::logger() : m_console(std::cerr) {
  // nop
}

void engine::args(int argc, char** argv) {
  instance().m_argc = argc;
  instance().m_argv = argv;
}

int engine::argc() {
  return instance().m_argc;
}

char** engine::argv() {
  return instance().m_argv;
}

void engine::add(const char* name, std::unique_ptr<test> t) {
  auto& suite = instance().m_suites[std::string{name ? name : ""}];
  for (auto& x : suite) {
    if (x->name() == t->name()) {
      std::cout << "duplicate test name: " << t->name() << '\n';
      std::abort();
    }
  }
  suite.emplace_back(std::move(t));
}

bool engine::run(bool colorize,
                 const std::string& log_file,
                 int verbosity_console,
                 int verbosity_file,
                 int, // TODO: max runtime
                 const std::string& suites_str,
                 const std::string& not_suites_str,
                 const std::string& tests_str,
                 const std::string& not_tests_str) {
  if (not_suites_str == "*" || not_tests_str == "*") {
    // nothing to do
    return true;
  }
  if (!colorize) {
    instance().m_reset = "";
    instance().m_black = "";
    instance().m_red = "";
    instance().m_green = "";
    instance().m_yellow = "";
    instance().m_blue = "";
    instance().m_magenta = "";
    instance().m_cyan = "";
    instance().m_white = "";
    instance().m_bold_black = "";
    instance().m_bold_red = "";
    instance().m_bold_green = "";
    instance().m_bold_yellow = "";
    instance().m_bold_blue = "";
    instance().m_bold_magenta = "";
    instance().m_bold_cyan = "";
    instance().m_bold_white = "";
  }
  if (!logger::init(verbosity_console, verbosity_file, log_file)) {
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
# if !defined(__clang__) && defined(__GNUC__)                                  \
     && __GNUC__ == 4 && __GNUC_MINOR__ < 9
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
    return !std::binary_search(blacklist.begin(), blacklist.end(), x)
           && (whitelist.empty()
               || std::binary_search(whitelist.begin(), whitelist.end(), x));
  };
# else
  std::regex suites{suites_str};
  std::regex tests{tests_str};
  std::regex not_suites;
  std::regex not_tests;
  // a default constructored regex matches is not equal to an "empty" regex
  if (!not_suites_str.empty()) {
    not_suites.assign(not_suites_str);
  }
  if (!not_tests_str.empty()) {
    not_tests.assign(not_tests_str);
  }
  auto enabled = [](const std::regex& whitelist,
                    const std::regex& blacklist,
                    const std::string& x) {
    // an empty whitelist means original input was "*", i.e., enable all
    return std::regex_search(x, whitelist)
           && !std::regex_search(x, blacklist);
  };
# endif
  for (auto& p : instance().m_suites) {
    if (!enabled(suites, not_suites, p.first)) {
      continue;
    }
    auto suite_name = p.first.empty() ? "<unnamed>" : p.first;
    auto pad = std::string((bar.size() - suite_name.size()) / 2, ' ');
    bool displayed_header = false;
    size_t tests_ran = 0;
    for (auto& t : p.second) {
      if (!enabled(tests, not_tests, t->name())) {
        continue;
      }
      instance().m_current_test = t.get();
      ++tests_ran;
      if (!displayed_header) {
        log.verbose() << color(yellow) << bar << '\n' << pad << suite_name
                      << '\n' << bar << color(reset) << "\n\n";
        displayed_header = true;
      }
      log.verbose() << color(yellow) << "- " << color(reset) << t->name()
                    << '\n';
      auto start = std::chrono::high_resolution_clock::now();
      watchdog::start();
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
                    << color(reset);
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
  auto percent_good =
    static_cast<unsigned>(double(100000 * total_good)
                          / double(total_good + total_bad)) / 1000.0;
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
    if (total_bad_expected) {
      log.info()
        << ' ' << color(cyan) << total_bad_expected << color(reset)
        << " failures expected";
    }
  }
  log.info() << '\n' << indent << "time:    " << color(yellow)
             << render(runtime) << '\n' << color(reset) << indent
             << "success: " << (total_bad > 0 ? color(green) : color(yellow))
             << percent_good << "%" << color(reset) << "\n\n" << color(cyan)
             << bar << color(reset) << '\n';
  return total_bad == total_bad_expected;
}

const char* engine::color(color_value v, color_face t) {
  if (v == reset) {
    return instance().m_reset;
  }
  switch (t) {
    default:
      return nullptr;
    case normal:
      switch (v) {
        default:
          return nullptr;
        case black:
          return instance().m_black;
        case red:
          return instance().m_red;
        case green:
          return instance().m_green;
        case yellow:
          return instance().m_yellow;
        case blue:
          return instance().m_blue;
        case magenta:
          return instance().m_magenta;
        case cyan:
          return instance().m_cyan;
        case white:
          return instance().m_white;
      }
    case bold:
      switch (v) {
        default:
          return nullptr;
        case black:
          return instance().m_bold_black;
        case red:
          return instance().m_bold_red;
        case green:
          return instance().m_bold_green;
        case yellow:
          return instance().m_bold_yellow;
        case blue:
          return instance().m_bold_blue;
        case magenta:
          return instance().m_bold_magenta;
        case cyan:
          return instance().m_bold_cyan;
        case white:
          return instance().m_bold_white;
      }
  }
}

const char* engine::last_check_file() {
  return instance().m_check_file;
}

void engine::last_check_file(const char* file) {
  instance().m_check_file = file;
}

size_t engine::last_check_line() {
  return instance().m_check_line;
}

void engine::last_check_line(size_t line) {
  instance().m_check_line = line;
}

test* engine::current_test() {
  return instance().m_current_test;
}

std::vector<std::string> engine::available_suites() {
  std::vector<std::string> result;
  for (auto& kvp : instance().m_suites) {
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

namespace detail {

expr::expr(test* parent, const char* filename, size_t lineno,
           bool should_fail, const char* expression)
    : m_test{parent},
      m_filename{filename},
      m_line{lineno},
      m_should_fail{should_fail},
      m_expr{expression} {
  assert(m_test != nullptr);
}

} // namespace detail
} // namespace test
} // namespace caf

int main(int argc, char** argv) {
  using namespace caf;
  // default values.
  int verbosity_console = 3;
  int verbosity_file = 3;
  int max_runtime = 10;
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
    return 0;
  }
  if (res.opts.count("available-suites") > 0) {
    std::cout << "available suites:" << std::endl;
    for (auto& s : test::engine::available_suites()) {
      std::cout << "  - " << s << std::endl;
    }
    return 0;
  }
  if (!res.remainder.empty()) {
    std::cerr << "*** invalid command line options" << std::endl
              << res.helptext << std::endl;
    return 1;
  }
  auto colorize = res.opts.count("no-colors") == 0;
  if (divider < argc) {
    test::engine::args(argc - divider - 1, argv + divider + 1);
  }
  auto result = test::engine::run(colorize, log_file, verbosity_console,
                                  verbosity_file, max_runtime, suites,
                                  not_suites, tests, not_tests);
  await_all_actors_done();
  shutdown();
  return result ? 0 : 1;
}

#endif // CAF_TEST_UNIT_TEST_IMPL_HPP
