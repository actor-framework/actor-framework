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

#ifndef CAF_TEST_UNIT_TEST_HPP
#define CAF_TEST_UNIT_TEST_HPP

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>

namespace caf {
namespace test {

/**
 * A sequence of *checks*.
 */
class test {
 public:
  test(std::string name);

  virtual ~test();

  size_t __expected_failures() const;

  void __pass(std::string msg);

  void __fail(std::string msg, bool expected);

  const std::vector<std::pair<bool, std::string>>& __trace() const;

  const std::string& __name() const;

  virtual void __run() = 0;

 private:
  size_t m_expected_failures = 0;
  std::vector<std::pair<bool, std::string>> m_trace;
  std::string m_name;
};

namespace detail {

// Thrown when a required check fails.
struct require_error : std::logic_error {
  require_error(const std::string& msg) : std::logic_error(msg) { }
  require_error(const require_error&) = default;
  require_error(require_error&&) = default;
  ~require_error() noexcept;
};

// Constructs spacing given a line number.
const char* fill(size_t line);

} // namespace detail

/**
 * Logs messages for the test framework.
 */
class logger {
 public:
  enum class level : int {
    quiet   = 0,
    error   = 1,
    info    = 2,
    verbose = 3,
    massive = 4
  };

  class message {
   public:
    message(logger& l, level lvl);

    template <class T>
    message& operator<<(const T& x) {
      m_logger.log(m_level, x);
      return *this;
    }

   private:
    logger& m_logger;
    level m_level;
  };

  static bool init(int lvl_cons, int lvl_file, const std::string& logfile);

  static logger& instance();

  template <class T>
  void log(level lvl, const T& x) {
    if (lvl <= m_level_console) {
      std::lock_guard<std::mutex> io_guard{m_console_mtx};
      m_console << x;
    }
    if (lvl <= m_level_file) {
      std::lock_guard<std::mutex> io_guard{m_file_mtx};
      m_file << x;
    }
  }

  message error();
  message info();
  message verbose();
  message massive();

 private:
  logger();

  level m_level_console;
  level m_level_file;
  std::ostream& m_console;
  std::ofstream m_file;
  std::mutex m_console_mtx;
  std::mutex m_file_mtx;
};

enum color_face {
  normal,
  bold
};

enum color_value {
  reset,
  black,
  red,
  green,
  yellow,
  blue,
  magenta,
  cyan,
  white
};

/**
 * Drives unit test execution.
 */
class engine {
  engine() = default;

 public:
  /**
   * Adds a test to the engine.
   * @param name The name of the suite.
   * @param t The test to register.
   */
  static void add(const char* name, std::unique_ptr<test> t);

  /**
   * Invokes tests in all suites.
   * @param colorize Whether to colorize the output.
   * @param log_file The filename of the log output. The empty string means
   *                 that no log file will be written.
   * @param verbosity_console The log verbosity on the console.
   * @param verbosity_file The log verbosity in the log file.
   * @param max_runtime The maximum number of seconds a test shall run.
   * @param suites The regular expression of the tests to run.
   * @param not_suites Whether to colorize the output.
   * @returns `true` iff all tests succeeded.
   */
  static bool run(bool colorize,
                  const std::string& log_file,
                  int verbosity_console,
                  int verbosity_file,
                  int max_runtime,
                  const std::regex& suites,
                  const std::regex& not_suites,
                  const std::regex& tests,
                  const std::regex& not_tests);

  /**
   * Retrieves a UNIX terminal color code or an empty string based on the
   * color configuration of the engine.
   */
  static const char* color(color_value v, color_face t = normal);

  static const char* last_check_file();
  static void last_check_file(const char* file);

  static size_t last_check_line();
  static void last_check_line(size_t line);

  static test* current_test();

 private:
  static engine& instance();

  static std::string render(std::chrono::microseconds t);

  const char* m_reset        = "\033[0m";
  const char* m_black        = "\033[30m";
  const char* m_red          = "\033[31m";
  const char* m_green        = "\033[32m";
  const char* m_yellow       = "\033[33m";
  const char* m_blue         = "\033[34m";
  const char* m_magenta      = "\033[35m";
  const char* m_cyan         = "\033[36m";
  const char* m_white        = "\033[37m";
  const char* m_bold_black   = "\033[1m\033[30m";
  const char* m_bold_red     = "\033[1m\033[31m";
  const char* m_bold_green   = "\033[1m\033[32m";
  const char* m_bold_yellow  = "\033[1m\033[33m";
  const char* m_bold_blue    = "\033[1m\033[34m";
  const char* m_bold_magenta = "\033[1m\033[35m";
  const char* m_bold_cyan    = "\033[1m\033[36m";
  const char* m_bold_white   = "\033[1m\033[37m";
  const char* m_check_file = "<none>";
  size_t m_check_line = 0;
  test* m_current_test = nullptr;
  std::map<std::string, std::vector<std::unique_ptr<test>>> m_suites;
};

namespace detail {

template <class T>
struct adder {
  adder(char const* name) {
    engine::add(name, std::unique_ptr<T>{new T});
  }
};

template <class T>
struct showable_base {};

template <class T>
std::ostream& operator<<(std::ostream& out, const showable_base<T>&) {
  out << engine::color(blue) << "<unprintable>" << engine::color(reset);
  return out;
}

template <class T>
class showable : public showable_base<T> {
 public:
  explicit showable(const T& x) : m_value(x) { }

  template <class U = T>
  friend auto operator<<(std::ostream& out, const showable& p)
    -> decltype(out << std::declval<const U&>()) {
    return out << p.m_value;
  }

 private:
  const T& m_value;
};

template <class T>
showable<T> show(T const &x) {
  return showable<T>{x};
}

template <class T>
struct lhs {
 public:
  lhs(test* parent, const char *file, size_t line, const char *expr,
      bool should_fail, const T& x)
    : m_test(parent),
      m_filename(file),
      m_line(line),
      m_expr(expr),
      m_should_fail(should_fail),
      m_value(x) {
  }

  ~lhs() {
    if (m_evaluated) {
      return;
    }
    if (eval(0)) {
      pass();
    } else {
      fail_unary();
    }
  }

  template <class U>
  using elevated =
    typename std::conditional<std::is_convertible<U, T>::value, T, U>::type;

  explicit operator bool() {
    m_evaluated = true;
    return !! m_value ? pass() : fail_unary();
  }

  template <class U>
  bool operator==(const U& u) {
    m_evaluated = true;
    return m_value == static_cast<elevated<U>>(u) ? pass() : fail(u);
  }

  template <class U>
  bool operator!=(const U& u) {
    m_evaluated = true;
    return m_value != static_cast<elevated<U>>(u) ? pass() : fail(u);
  }

  template <class U>
  bool operator<(const U& u) {
    m_evaluated = true;
    return m_value < static_cast<elevated<U>>(u) ? pass() : fail(u);
  }

  template <class U>
  bool operator<=(const U& u) {
    m_evaluated = true;
    return m_value <= static_cast<elevated<U>>(u) ? pass() : fail(u);
  }

  template <class U>
  bool operator>(const U& u) {
    m_evaluated = true;
    return m_value > static_cast<elevated<U>>(u) ? pass() : fail(u);
  }

  template <class U>
  bool operator>=(const U& u) {
    m_evaluated = true;
    return m_value >= static_cast<elevated<U>>(u) ? pass() : fail(u);
  }

 private:
  template<class V = T>
  auto eval(int) -> decltype(! std::declval<V>()) {
    return !! m_value;
  }

  bool eval(long) {
    return true;
  }

  bool pass() {
    m_passed = true;
    std::stringstream ss;
    ss
      << engine::color(green) << "** "
      << engine::color(blue) << m_filename << engine::color(yellow) << ":"
      << engine::color(blue) << m_line << fill(m_line) << engine::color(reset)
      << m_expr;
    m_test->__pass(ss.str());
    return true;
  }

  bool fail_unary() {
    std::stringstream ss;
    ss
      << engine::color(red) << "!! "
      << engine::color(blue) << m_filename << engine::color(yellow) << ":"
      << engine::color(blue) << m_line << fill(m_line) << engine::color(reset)
      << m_expr;
    m_test->__fail(ss.str(), m_should_fail);
    return false;
  }

  template <class U>
  bool fail(const U& u) {
    std::stringstream ss;
    ss
      << engine::color(red) << "!! "
      << engine::color(blue) << m_filename << engine::color(yellow) << ":"
      << engine::color(blue) << m_line << fill(m_line) << engine::color(reset)
      << m_expr << engine::color(magenta) << " ("
      << engine::color(red) << show(m_value) << engine::color(magenta) << " !! "
      << engine::color(red) << show(u) << engine::color(magenta) << ')'
      << engine::color(reset);
    m_test->__fail(ss.str(), m_should_fail);
    return false;
  }

  bool m_evaluated = false;
  bool m_passed = false;
  test* m_test;
  const char *m_filename;
  size_t m_line;
  const char *m_expr;
  bool m_should_fail;
  const T& m_value;
};

struct expr {
 public:
  expr(test* parent, const char* filename, size_t lineno, bool should_fail,
       const char* expression);

  template <class T>
  lhs<T> operator->*(const T& x) {
    return {m_test, m_filename, m_line, m_expr, m_should_fail, x};
  }

private:
  test* m_test;
  const char* m_filename;
  size_t m_line;
  bool m_should_fail;
  const char* m_expr;
};

} // namespace detail
} // namespace test
} // namespace caf

#define CAF_TEST_ERROR(msg) \
  ::caf::test::logger::instance().error() << msg << '\n'
#define CAF_TEST_INFO(msg) \
  ::caf::test::logger::instance().info() << msg << '\n'
#define CAF_TEST_VERBOSE(msg) \
  ::caf::test::logger::instance().verbose() << msg << '\n'

#define CAF_PASTE_CONCAT(lhs, rhs) lhs ## rhs
#define CAF_PASTE(lhs, rhs) CAF_PASTE_CONCAT(lhs, rhs)
#define CAF_UNIQUE(name) CAF_PASTE(name, __LINE__)

#ifndef CAF_SUITE
#define CAF_SUITE ""
#endif

#define CAF_CHECK(...)                                                      \
  do {                                                                      \
    (void)(::caf::test::detail::expr{                                       \
             ::caf::test::engine::current_test(), __FILE__, __LINE__,       \
             false, #__VA_ARGS__} ->* __VA_ARGS__);                         \
    ::caf::test::engine::last_check_file(__FILE__);                         \
    ::caf::test::engine::last_check_line(__LINE__);                         \
  } while (false)

#define CAF_FAIL(...)                                                       \
  do {                                                                      \
    (void)(::caf::test::detail::expr{                                       \
             ::caf::test::engine::current_test(), __FILE__, __LINE__,       \
             true, #__VA_ARGS__} ->* __VA_ARGS__);                          \
    ::caf::test::engine::last_check_file(__FILE__);                         \
    ::caf::test::engine::last_check_line(__LINE__);                         \
  } while (false)

#define CAF_REQUIRE(...)                                                    \
  do {                                                                      \
    auto CAF_UNIQUE(__result) =                                             \
      ::caf::test::detail::expr{::caf::test::engine::current_test(),        \
      __FILE__, __LINE__, false, #__VA_ARGS__} ->* __VA_ARGS__;             \
    if (! CAF_UNIQUE(__result)) {                                           \
      throw ::caf::test::detail::require_error{#__VA_ARGS__};               \
    }                                                                       \
    ::caf::test::engine::last_check_file(__FILE__);                         \
    ::caf::test::engine::last_check_line(__LINE__);                         \
  } while (false)

#define CAF_TEST(name)                                                      \
  namespace {                                                               \
  struct CAF_UNIQUE(test) : public ::caf::test::test {                      \
    CAF_UNIQUE(test)() : test{name} { }                                     \
    void __run() final;                                                     \
  };                                                                        \
  ::caf::test::detail::adder<CAF_UNIQUE(test)> CAF_UNIQUE(a){CAF_SUITE};    \
  } /* namespace <anonymous> */                                             \
  void CAF_UNIQUE(test)::__run()

// Boost Test compatibility macros
#define CAF_CHECK_EQUAL(x, y) CAF_CHECK(x == y)
#define CAF_CHECK_NE(x, y) CAF_CHECK(x != y)
#define CAF_CHECK_LT(x, y) CAF_CHECK(x < y)
#define CAF_CHECK_LE(x, y) CAF_CHECK(x <= y)
#define CAF_CHECK_GT(x, y) CAF_CHECK(x > y)
#define CAF_CHECK_GE(x, y) CAF_CHECK(x >= y)

#endif // CAF_TEST_UNIT_TEST_HPP
