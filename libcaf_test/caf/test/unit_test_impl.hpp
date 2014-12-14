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

#include <cassert>
#include <condition_variable>
#include <thread>

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

test::test(std::string name) : m_name{std::move(name)} {
  // nop
}

test::~test() {
  // nop
}

size_t test::__expected_failures() const {
  return m_expected_failures;
}

void test::__pass(std::string msg) {
  m_trace.emplace_back(true, std::move(msg));
}

void test::__fail(std::string msg, bool expected) {
  m_trace.emplace_back(false, std::move(msg));
  if (expected) {
    ++m_expected_failures;
  }
}

const std::vector<std::pair<bool, std::string>>& test::__trace() const {
  return m_trace;
}

const std::string& test::__name() const {
  return m_name;
}


namespace detail {

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

logger::message::message(logger& l, level lvl)
    : m_logger(l),
      m_level(lvl) {
}

bool logger::init(int lvl_cons, int lvl_file,
                         const std::string& logfile) {
  instance().m_level_console = static_cast<level>(lvl_cons);
  instance().m_level_file = static_cast<level>(lvl_file);
  if (! logfile.empty()) {
    instance().m_file.open(logfile, std::ofstream::out | std::ofstream::app);
    return !! instance().m_file;
  }
  return true;
}

logger& logger::instance() {
  static logger l;
  return l;
}

logger::message logger::error() {
  return message{*this, level::error};
}

logger::message logger::info() {
  return message{*this, level::info};
}

logger::message logger::verbose() {
  return message{*this, level::verbose};
}

logger::message logger::massive() {
  return message{*this, level::massive};
}

logger::logger() : m_console(std::cerr) {}


void engine::add(const char* name, std::unique_ptr<test> t) {
  auto& suite = instance().m_suites[std::string{name ? name : ""}];
  for (auto& x : suite)
    if (x->__name() == t->__name()) {
      std::cout << "duplicate test name: " << t->__name() << '\n';
      std::abort();
    }
  suite.emplace_back(std::move(t));
}

bool engine::run(bool colorize,
                        const std::string& log_file,
                        int verbosity_console,
                        int verbosity_file,
                        int,
                        const std::regex& suites,
                        const std::regex& not_suites,
                        const std::regex& tests,
                        const std::regex& not_tests) {
  if (! colorize) {
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
  if (! logger::init(verbosity_console, verbosity_file, log_file))
    return false;
  auto& log = logger::instance();
  std::chrono::microseconds runtime{0};
  size_t total_suites = 0;
  size_t total_tests = 0;
  size_t total_good = 0;
  size_t total_bad = 0;
  size_t total_bad_expected = 0;
  auto bar = '+' + std::string(70, '-') + '+';
  auto failed_require = false;
  for (auto& p : instance().m_suites) {
    if (! std::regex_search(p.first, suites) ||
        std::regex_search(p.first, not_suites))
      continue;
    auto suite_name = p.first.empty() ? "<unnamed>" : p.first;
    auto pad = std::string((bar.size() - suite_name.size()) / 2, ' ');
    bool displayed_header = false;
    size_t tests_ran = 0;
    for (auto& t : p.second) {
      if (! std::regex_search(t->__name(), tests)
          || std::regex_search(t->__name(), not_tests))
        continue;
      instance().m_current_test = t.get();
      ++tests_ran;
      if (! displayed_header) {
        log.verbose()
          << color(yellow) << bar << '\n' << pad << suite_name << '\n' << bar
          << color(reset) << "\n\n";
        displayed_header = true;
      }
      log.verbose()
          << color(yellow) << "- " << color(reset) << t->__name() << '\n';
      auto start = std::chrono::high_resolution_clock::now();
      watchdog::start();
      try {
        t->__run();
      } catch (const detail::require_error&) {
        failed_require = true;
      }
      watchdog::stop();
      auto stop = std::chrono::high_resolution_clock::now();
      auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
      runtime += elapsed;
      ++total_tests;
      size_t good = 0;
      size_t bad = 0;
      for (auto& trace : t->__trace()) {
        if (trace.first) {
          ++good;
          log.massive() << "  " << trace.second << '\n';
        } else {
          ++bad;
          log.error() << "  " << trace.second << '\n';
        }
      }
      if (failed_require) {
        log.error()
          << color(red) << "     REQUIRED" << color(reset) << '\n'
          << "     " << color(blue) << last_check_file() << color(yellow)
          << ":" << color(cyan) << last_check_line() << color(reset)
          << detail::fill(last_check_line()) << "had last successful check"
          << '\n';
      }
      total_good += good;
      total_bad += bad;
      total_bad_expected += t->__expected_failures();
      log.verbose()
          << color(yellow) << "  -> " << color(cyan) << good + bad
          << color(reset) << " check" << (good + bad > 1 ? "s " : " ")
          << "took " << color(cyan) << render(elapsed) << color(reset);
      if (bad > 0)
        log.verbose()
          << " (" << color(green) << good << color(reset) << '/'
          << color(red) << bad << color(reset) << ")" << '\n';
      else
        log.verbose() << '\n';
      if (failed_require)
        break;
    }
    // We only counts suites for which have executed one or more tests.
    if (tests_ran > 0)
      ++total_suites;
    if (displayed_header)
      log.verbose() << '\n';
    if (failed_require)
      break;
  }
  auto percent_good =
    static_cast<unsigned>(double(100000 * total_good) 
                          / double(total_good + total_bad)) / 1000.0;
  auto title = std::string{"summary"};
  auto pad = std::string((bar.size() - title.size()) / 2, ' ');
  auto indent = std::string(24, ' ');
  log.info()
    << color(cyan) << bar << '\n' << pad << title << '\n' << bar
    << color(reset) << "\n\n"
    << indent << "suites:  " << color(yellow) << total_suites << color(reset)
    << '\n' << indent << "tests:   " << color(yellow) << total_tests
    << color(reset) << '\n' << indent << "checks:  " << color(yellow)
    << total_good + total_bad << color(reset);
  if (total_bad > 0) {
    log.info()
    << " (" << color(green) << total_good << color(reset) << '/'
    << color(red) << total_bad << color(reset) << ")";
    if (total_bad_expected)
      log.info()
        << ' ' << color(cyan) << total_bad_expected << color(reset)
        << " failures expected";
  }
  log.info()
    << '\n' << indent << "time:    " << color(yellow) << render(runtime)
    << '\n' << color(reset) << indent << "success: "
    << (total_bad > 0 ? color(green) : color(yellow))
    << percent_good << "%" << color(reset) << "\n\n"
    << color(cyan) << bar << color(reset) << '\n';
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

#endif // CAF_TEST_UNIT_TEST_IMPL_HPP
