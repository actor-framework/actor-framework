// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <thread>

#ifndef CAF_WINDOWS
#  include <unistd.h>
#endif

#include "caf/test/unit_test.hpp"

#include "caf/config_option_adder.hpp"
#include "caf/config_option_set.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/settings.hpp"
#include "caf/string_algorithms.hpp"

namespace caf::test {
inline namespace legacy {

#ifdef CAF_ENABLE_EXCEPTIONS

requirement_error::requirement_error(std::string msg) : what_(std::move(msg)) {
  // nop
}

const char* requirement_error::what() const noexcept {
  return what_.c_str();
}

#endif // CAF_ENABLE_EXCEPTIONS

class watchdog {
public:
  static void start(int secs);
  static void stop();

private:
  watchdog(int secs) {
    thread_ = std::thread{[this, secs] {
      caf::detail::set_thread_name("test.watchdog");
      auto tp = std::chrono::high_resolution_clock::now()
                + std::chrono::seconds(secs);
      std::unique_lock<std::mutex> guard{mtx_};
      while (!canceled_
             && cv_.wait_until(guard, tp) != std::cv_status::timeout) {
        // spin
      }
      if (!canceled_) {
        logger::instance().error()
          << "WATCHDOG: unit test did not finish within " << secs
          << "s, abort\n";
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

namespace {
watchdog* s_watchdog;
}

void watchdog::start(int secs) {
  if (secs > 0)
    s_watchdog = new watchdog(secs);
}

void watchdog::stop() {
  delete s_watchdog;
}

test::test(std::string test_name, bool disabled_by_default)
  : expected_failures_(0),
    name_(std::move(test_name)),
    good_(0),
    bad_(0),
    disabled_(disabled_by_default) {
  // nop
}

test::~test() {
  // nop
}

size_t test::expected_failures() const {
  return expected_failures_;
}

void test::pass() {
  ++good_;
}

void test::fail(bool expected) {
  ++bad_;
  if (expected)
    ++expected_failures_;
}

const std::string& test::name() const {
  return name_;
}

namespace detail {

[[noreturn]] void requirement_failed(const std::string& msg) {
  auto& log = logger::instance();
  log.error() << term::red << "     REQUIRED: " << msg << term::reset << '\n'
              << "     " << term::blue << engine::last_check_file()
              << term::yellow << ":" << term::cyan << engine::last_check_line()
              << term::reset << detail::fill(engine::last_check_line())
              << "had last successful check" << '\n';
  CAF_RAISE_ERROR(requirement_error, msg.c_str());
  abort();
}

const char* fill(size_t line) {
  if (line < 10)
    return "    ";
  if (line < 100)
    return "   ";
  if (line < 1000)
    return "  ";
  return " ";
}

void remove_trailing_spaces(std::string& x) {
  x.erase(std::find_if_not(x.rbegin(), x.rend(), ::isspace).base(), x.end());
}

bool check(test* parent, const char* file, size_t line, const char* expr,
           bool should_fail, bool result) {
  auto out = logger::instance().massive();
  if (result) {
    out << term::green << "** " << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::reset << expr << '\n';
    parent->pass();
  } else {
    out << term::red << "!! " << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::reset << expr << '\n';
    parent->fail(should_fail);
  }
  engine::last_check_file(file);
  engine::last_check_line(line);
  return result;
}

bool check_un(bool result, const char* file, size_t line, const char* expr) {
  using namespace std::literals;
  auto rel_up = "../"sv;
  while (strncmp(file, rel_up.data(), rel_up.size()) == 0)
    file += rel_up.size();
  auto parent = engine::current_test();
  auto out = logger::instance().massive();
  if (result) {
    out << term::green << "** " << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::reset << "passed" << '\n';
    parent->pass();
  } else {
    out << term::red << "!! " << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::red
        << "check failed: " << expr << term::reset << '\n';
    parent->fail(false);
  }
  engine::last_check_file(file);
  engine::last_check_line(line);
  return result;
}

bool check_bin(bool result, const char* file, size_t line, const char* expr,
               const std::string& lhs, const std::string& rhs) {
  using namespace std::literals;
  auto rel_up = "../"sv;
  while (strncmp(file, rel_up.data(), rel_up.size()) == 0)
    file += rel_up.size();
  auto parent = engine::current_test();
  auto out = logger::instance().massive();
  if (result) {
    out << term::green << "** " << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::reset << "passed" << '\n';
    parent->pass();
  } else {
    out << term::red << "!! " << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::red
        << "check failed: " << expr << term::reset << '\n'
        << "  lhs: " << lhs << '\n'
        << "  rhs: " << rhs << '\n';
    parent->fail(false);
  }
  engine::last_check_file(file);
  engine::last_check_line(line);
  return result;
}

void require_un(bool result, const char* file, size_t line, const char* expr) {
  using namespace std::literals;
  auto rel_up = "../"sv;
  while (strncmp(file, rel_up.data(), rel_up.size()) == 0)
    file += rel_up.size();
  auto parent = engine::current_test();
  auto out = logger::instance().massive();
  if (result) {
    out << term::green << "** " << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::reset << "passed" << '\n';
    parent->pass();
    engine::last_check_file(file);
    engine::last_check_line(line);
  } else {
    out << term::red << "!! " << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::red
        << "requirement failed: " << expr << term::reset << '\n';
    parent->fail(false);
    std::string msg = "requirement failed in ";
    msg += file;
    msg += " line ";
    msg += std::to_string(line);
    requirement_failed(std::move(msg));
  }
}

void require_bin(bool result, const char* file, size_t line, const char* expr,
                 const std::string& lhs, const std::string& rhs) {
  using namespace std::literals;
  auto rel_up = "../"sv;
  while (strncmp(file, rel_up.data(), rel_up.size()) == 0)
    file += rel_up.size();
  auto parent = engine::current_test();
  auto out = logger::instance().massive();
  if (result) {
    out << term::green << "** " << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::reset << "passed" << '\n';
    parent->pass();
    engine::last_check_file(file);
    engine::last_check_line(line);
  } else {
    out << term::red << "!! " << term::blue << file << term::yellow << ":"
        << term::blue << line << fill(line) << term::red
        << "requirement failed: " << expr << term::reset << '\n'
        << "  lhs: " << lhs << '\n'
        << "  rhs: " << rhs << '\n';
    parent->fail(false);
    std::string msg = "requirement failed in ";
    msg += file;
    msg += " line ";
    msg += std::to_string(line);
    requirement_failed(std::move(msg));
  }
}

} // namespace detail

logger::stream::stream(logger& parent, logger::level lvl)
  : parent_(parent), lvl_(lvl) {
  // nop
}

bool logger::init(int lvl_cons, int lvl_file, const std::string& logfile) {
  instance().level_console_ = static_cast<level>(lvl_cons);
  instance().level_file_ = static_cast<level>(lvl_file);
  if (!logfile.empty()) {
    instance().file_.open(logfile, std::ofstream::out | std::ofstream::app);
    return !!instance().file_;
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

void logger::disable_colors() {
  // Disable colors by piping all writes through dummy_.
  // Since dummy_ is not a TTY, colors are turned off implicitly.
  dummy_.copyfmt(std::cerr);
  dummy_.clear(std::cerr.rdstate());
  dummy_.basic_ios<char>::rdbuf(std::cerr.rdbuf());
  console_ = &dummy_;
}

logger::logger()
  : level_console_(level::error),
    level_file_(level::error),
    console_(&std::cerr) {
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
  std::string name = cstr_name != nullptr ? cstr_name : "";
  auto& suite = instance().suites_[name];
  for (auto& x : suite) {
    if (x->name() == ptr->name()) {
      std::cerr << "duplicate test name: " << ptr->name() << std::endl;
      std::abort();
    }
  }
  suite.emplace_back(std::move(ptr));
}

bool engine::run(bool colorize, const std::string& log_file,
                 int verbosity_console, int verbosity_file,
                 const std::string& suites_str,
                 const std::string& not_suites_str,
                 const std::string& tests_str,
                 const std::string& not_tests_str) {
  if (not_suites_str == "*" || not_tests_str == "*") {
    // nothing to do
    return true;
  }
  instance().colorize_ = colorize;
  if (!logger::init(verbosity_console, verbosity_file, log_file)) {
    return false;
  }
  auto& log = logger::instance();
  if (!colorize)
    log.disable_colors();
  std::chrono::microseconds runtime{0};
  size_t total_suites = 0;
  size_t total_tests = 0;
  size_t total_good = 0;
  size_t total_bad = 0;
  size_t total_bad_expected = 0;
  auto bar = '+' + std::string(70, '-') + '+';
  std::regex suites{suites_str};
  std::regex tests{tests_str};
  std::regex not_suites;
  std::regex not_tests;
  // a default constructored regex matches is not equal to an "empty" regex
  if (!not_suites_str.empty())
    not_suites.assign(not_suites_str);
  if (!not_tests_str.empty())
    not_tests.assign(not_tests_str);
  auto enabled = [](const std::regex& selected, const std::regex& blocked,
                    const std::string& x) {
    return std::regex_search(x, selected) && !std::regex_search(x, blocked);
  };
  auto test_enabled = [&](const std::regex& selected, const std::regex& blocked,
                          const test& x) {
    // Disabled tests run if explicitly requested by the user, i.e.,
    // tests_str is not the ".*" catch-all default.
    return (!x.disabled() || tests_str != ".*")
           && enabled(selected, blocked, x.name());
  };
  std::vector<std::string> failed_tests;
  for (auto& p : instance().suites_) {
    if (!enabled(suites, not_suites, p.first))
      continue;
    auto suite_name = p.first.empty() ? "<unnamed>" : p.first;
    auto pad = std::string((bar.size() - suite_name.size()) / 2, ' ');
    bool displayed_header = false;
    size_t tests_ran = 0;
    for (auto& t : p.second) {
      if (!test_enabled(tests, not_tests, *t))
        continue;
      instance().current_test_ = t.get();
      ++tests_ran;
      if (!displayed_header) {
        log.verbose() << term::yellow << bar << '\n'
                      << pad << suite_name << '\n'
                      << bar << term::reset << "\n\n";
        displayed_header = true;
      }
      log.verbose() << term::yellow << "- " << term::reset << t->name() << '\n';
      auto start = std::chrono::high_resolution_clock::now();
      watchdog::start(max_runtime());
#ifdef CAF_ENABLE_EXCEPTIONS
      try {
        t->run_test_impl();
      } catch (requirement_error&) {
        // nop
      } catch (std::exception& ex) {
        t->fail(false);
        log.error() << term::red << "!! uncaught exception, what: " << ex.what()
                    << term::reset_endl;
      } catch (...) {
        t->fail(false);
        log.error() << term::red << "!! uncaught exception of unknown type"
                    << term::reset_endl;
      }
#else
      t->run_test_impl();
#endif // CAF_ENABLE_EXCEPTIONS
      watchdog::stop();
      auto stop = std::chrono::high_resolution_clock::now();
      auto elapsed
        = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
      runtime += elapsed;
      ++total_tests;
      size_t good = t->good();
      size_t bad = t->bad();
      total_good += good;
      total_bad += bad;
      total_bad_expected += t->expected_failures();
      log.verbose() << term::yellow << "  -> " << term::cyan << good + bad
                    << term::reset << " check" << (good + bad > 1 ? "s " : " ")
                    << "took " << term::cyan << render(elapsed) << term::reset
                    << '\n';
      if (bad > 0) {
        // concat suite name + test name
        failed_tests.emplace_back(p.first);
        failed_tests.back() += ":";
        failed_tests.back() += t->name();
        log.verbose() << " (" << term::green << good << term::reset << '/'
                      << term::red << bad << term::reset << ")" << '\n';
      } else {
        log.verbose() << '\n';
      }
    }
    // only count suites which have executed one or more tests
    if (tests_ran > 0) {
      ++total_suites;
    }
    if (displayed_header)
      log.verbose() << '\n';
  }
  unsigned percent_good = 100;
  if (total_bad > 0) {
    auto tmp
      = (100000.0 * static_cast<double>(total_good))
        / static_cast<double>(total_good + total_bad - total_bad_expected);
    percent_good = static_cast<unsigned>(tmp / 1000.0);
  }
  auto title = std::string{"summary"};
  auto pad = std::string((bar.size() - title.size()) / 2, ' ');
  auto indent = std::string(24, ' ');
  log.info() << term::cyan << bar << '\n'
             << pad << title << '\n'
             << bar << term::reset << "\n\n"
             << indent << "suites:  " << term::yellow << total_suites
             << term::reset << '\n'
             << indent << "tests:   " << term::yellow << total_tests
             << term::reset << '\n'
             << indent << "checks:  " << term::yellow << total_good + total_bad
             << term::reset;
  if (total_bad > 0) {
    log.info() << " (" << term::green << total_good << term::reset << '/'
               << term::red << total_bad << term::reset << ")";
    if (total_bad_expected > 0) {
      log.info() << ' ' << term::cyan << total_bad_expected << term::reset
                 << " failures expected";
    }
  }
  log.info() << '\n'
             << indent << "time:    " << term::yellow << render(runtime) << '\n'
             << term::reset << indent << "success: "
             << (total_bad == total_bad_expected ? term::green : term::red)
             << percent_good << "%" << term::reset << "\n\n";
  if (!failed_tests.empty()) {
    log.info() << indent << "failed tests:" << '\n';
    for (auto& name : failed_tests)
      log.info() << indent << "- " << name << '\n';
    log.info() << '\n';
  }
  log.info() << term::cyan << bar << term::reset << '\n';
  return total_bad == total_bad_expected;
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
  for (auto& kvp : instance().suites_)
    result.push_back(kvp.first);
  return result;
}

std::vector<std::string> engine::available_tests(const std::string& suite) {
  std::vector<std::string> result;
  auto i = instance().suites_.find(suite);
  if (i == instance().suites_.end())
    return result;
  for (auto& ptr : i->second)
    result.push_back(ptr->name());
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
         : t.count() > 1000 ? (std::to_string(t.count() / 1000) + " ms")
                            : (std::to_string(t.count()) + " us");
}

int main(int argc, char** argv) {
  // set path of executable
  engine::path(argv[0]);
  // default values.
  int verbosity_console = 3;
  int verbosity_file = 3;
  int max_runtime = engine::max_runtime();
  std::string log_file;
  std::string suites = ".*";
  std::string not_suites;
  std::string tests = ".*";
  std::string not_tests;
  std::string suite_query;
  // use all arguments after '--' for the test engine.
  std::string delimiter = "--";
  auto divider = argc;
  for (auto i = 1; i < argc; ++i) {
    if (delimiter == argv[i]) {
      divider = i;
      break;
    }
  }
  caf::config_option_set options;
  caf::config_option_adder{options, "global"}
    .add<bool>("no-colors,n", "disable coloring (ignored on Windows)")
    .add<bool>("help,h?", "print this help text")
    .add<bool>("available-suites,a", "print available suites")
    .add(log_file, "log-file,l", "set output file")
    .add(verbosity_console, "console-verbosity,v",
         "set verbosity level of console (1-5)")
    .add(verbosity_file, "file-verbosity,V",
         "set verbosity level of file output (1-5)")
    .add(max_runtime, "max-runtime,r",
         "set maximum runtime in seconds (0 = infinite)")
    .add(suites, "suites,s",
         "define what suites to run, either * or a comma-separated list")
    .add(not_suites, "not-suites,S", "exclude suites")
    .add(tests, "tests,t", "set tests")
    .add(not_tests, "not-tests,T", "exclude tests")
    .add(suite_query, "available-tests,A",
         "print available tests for given suite");
  caf::settings conf;
  std::vector<std::string> args_cpy{argv + 1, argv + divider};
  auto res = options.parse(conf, args_cpy);
  if (res.first != caf::pec::success) {
    std::cerr << "error while parsing argument \"" << *res.second
              << "\": " << to_string(res.first) << "\n\n";
    std::cerr << options.help_text() << std::endl;
    return EXIT_FAILURE;
  }
  if (caf::get_or(conf, "help", false)) {
    std::cout << options.help_text() << std::endl;
    return EXIT_SUCCESS;
  }
  if (!suite_query.empty()) {
    std::cout << "available tests in suite " << suite_query << ":" << std::endl;
    for (auto& t : engine::available_tests(suite_query))
      std::cout << "  - " << t << std::endl;
    return 0;
  }
  if (caf::get_or(conf, "available-suites", false)) {
    std::cout << "available suites:" << std::endl;
    for (auto& s : engine::available_suites())
      std::cout << "  - " << s << std::endl;
    return EXIT_SUCCESS;
  }
  auto colorize = !caf::get_or(conf, "no-colors", false);
  std::vector<char*> args;
  if (divider < argc) {
    // make a new args vector that contains argv[0] and all remaining args
    args.push_back(argv[0]);
    for (int i = divider + 1; i < argc; ++i)
      args.push_back(argv[i]);
    engine::args(static_cast<int>(args.size()), args.data());
  } else {
    engine::args(1, argv);
  }
  engine::max_runtime(max_runtime);
  auto result = engine::run(colorize, log_file, verbosity_console,
                            verbosity_file, suites, not_suites, tests,
                            not_tests);
  return result ? 0 : 1;
}

} // namespace legacy
} // namespace caf::test

#ifndef CAF_TEST_NO_MAIN
int main(int argc, char** argv) {
  caf::core::init_global_meta_objects();
  return caf::test::main(argc, argv);
}
#endif // CAF_TEST_UNIT_TEST_IMPL_HPP
