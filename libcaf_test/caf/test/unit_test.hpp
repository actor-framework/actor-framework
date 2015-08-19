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
#include <cmath>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>

namespace caf {
class message;
namespace test {

/// Default test-running function.
/// This function will be called automatically unless you define
/// `CAF_TEST_NO_MAIN` before including `caf/test/unit_test.hpp`. In
/// the latter case you will have to provide you own `main` function,
/// where you may want to call `caf::test::main` from.
int main(int argc, char** argv);

/// A sequence of *checks*.
class test {
public:
  test(std::string name);

  virtual ~test();

  size_t expected_failures() const;

  void pass(std::string msg);

  void fail(std::string msg, bool expected);

  const std::string& name() const;

  inline size_t good() {
    return good_;
  }

  inline size_t bad() {
    return bad_;
  }

  virtual void run() = 0;

private:
  size_t expected_failures_;
  std::string name_;
  size_t good_;
  size_t bad_;
};

struct dummy_fixture { };

template <class T>
class test_impl : public test {
public:
  test_impl(std::string test_name) : test(std::move(test_name)) {
    // nop
  }

  virtual void run() override {
    T impl;
    impl.run();
  }
};

namespace detail {

// thrown when a required check fails
class require_error : std::logic_error {
public:
  require_error(const std::string& msg);
  require_error(const require_error&) = default;
  require_error(require_error&&) = default;
  ~require_error() noexcept;
};

// constructs spacing given a line number.
const char* fill(size_t line);

} // namespace detail

/// Logs messages for the test framework.
class logger {
public:
  enum class level : int {
    quiet   = 0,
    error   = 1,
    info    = 2,
    verbose = 3,
    massive = 4
  };

  /// Output stream for logging purposes.
  class stream {
  public:
    stream(logger& l, level lvl);

    stream(stream&&);

    template <class T>
    typename std::enable_if<
      ! std::is_same<T, char*>::value,
      stream&
    >::type
    operator<<(const T& x) {
      buf_ << x;
      return *this;
    }

    stream& operator<<(const char& c);

    stream& operator<<(const char* cstr);

    stream& operator<<(const std::string& str);

  private:
    void flush();

    logger& logger_;
    level level_;
    std::ostringstream buf_;
  };

  static bool init(int lvl_cons, int lvl_file, const std::string& logfile);

  static logger& instance();

  template <class T>
  void log(level lvl, const T& x) {
    if (lvl <= level_console_) {
      std::lock_guard<std::mutex> io_guard{console_mtx_};
      console_ << x;
    }
    if (lvl <= level_file_) {
      std::lock_guard<std::mutex> io_guard{file_mtx_};
      file_ << x;
    }
  }

  stream error();
  stream info();
  stream verbose();
  stream massive();

private:
  logger();

  level level_console_;
  level level_file_;
  std::ostream& console_;
  std::ofstream file_;
  std::mutex console_mtx_;
  std::mutex file_mtx_;
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

/// Drives unit test execution.
class engine {
public:
  /// Sets external command line arguments.
  /// @param argc The argument counter.
  /// @param argv The argument vectors.
  static void args(int argc, char** argv);

  /// Retrieves the argument counter.
  /// @returns The number of arguments set via ::args or 0.
  static int argc();

  /// Retrieves the argument vector.
  /// @returns The argument vector set via ::args or `nullptr`.
  static char** argv();

  /// Sets path of current executable.
  /// @param argv The path of current executable.
  static void path(char* argv);

  /// Retrieves the path of current executable
  /// @returns The path to executable set via ::path(char*) or `nullptr`.
  static char* path();

  /// Returns the maximum number of seconds a test case is allowed to run.
  static int max_runtime();

  /// Sets the maximum number of seconds a test case is
  /// allowed to run to `value`.
  static void max_runtime(int value);

  /// Adds a test to the engine.
  /// @param name The name of the suite.
  /// @param ptr The test to register.
  static void add(const char* name, std::unique_ptr<test> ptr);

  /// Invokes tests in all suites.
  /// @param colorize Whether to colorize the output.
  /// @param log_file The filename of the log output. The empty string means
  ///                 that no log file will be written.
  /// @param verbosity_console The log verbosity on the console.
  /// @param verbosity_file The log verbosity in the log file.
  /// @param suites The regular expression of the tests to run.
  /// @param not_suites Whether to colorize the output.
  /// @returns `true` iff all tests succeeded.
  static bool run(bool colorize,
                  const std::string& log_file,
                  int verbosity_console,
                  int verbosity_file,
                  const std::string& suites,
                  const std::string& not_suites,
                  const std::string& tests,
                  const std::string& not_tests);

  /// Retrieves a UNIX terminal color code or an empty string based on the
  /// color configuration of the engine.
  static const char* color(color_value v, color_face t = normal);

  static const char* last_check_file();
  static void last_check_file(const char* file);

  static size_t last_check_line();
  static void last_check_line(size_t line);

  static test* current_test();

  static std::vector<std::string> available_suites();

private:
  engine() = default;

  static engine& instance();

  static std::string render(std::chrono::microseconds t);

  int argc_ = 0;
  char** argv_ = nullptr;
  char*  path_ = nullptr;
  const char* colors_[9][2] = {
    {"\033[0m", "\033[0m"},          // reset
    {"\033[30m", "\033[1m\033[30m"}, // black
    {"\033[31m", "\033[1m\033[31m"}, // red
    {"\033[32m", "\033[1m\033[32m"}, // green
    {"\033[33m", "\033[1m\033[33m"}, // yellow
    {"\033[34m", "\033[1m\033[34m"}, // blue
    {"\033[35m", "\033[1m\033[35m"}, // magenta
    {"\033[36m", "\033[1m\033[36m"}, // cyan
    {"\033[37m", "\033[1m\033[37m"}  // white
  };
  const char* check_file_ = "<none>";
  size_t check_line_ = 0;
  test* current_test_ = nullptr;
  std::map<std::string, std::vector<std::unique_ptr<test>>> suites_;
  int max_runtime_ = 30; // 30s per default
};

namespace detail {

template <class T>
struct adder {
  adder(const char* suite_name, const char* test_name) {
    engine::add(suite_name, std::unique_ptr<T>{new T(test_name)});
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
  explicit showable(const T& x) : value_(x) { }

  template <class U = T>
  friend auto operator<<(std::ostream& out, const showable& p)
    -> decltype(out << std::declval<const U&>()) {
    return out << p.value_;
  }

private:
  const T& value_;
};

template <class T>
showable<T> show(T const &x) {
  return showable<T>{x};
}

template <class T,
          bool IsFloat = std::is_floating_point<T>::value,
          bool IsIntegral = std::is_integral<T>::value>
class lhs_cmp {
public:
  template <class U>
  bool operator()(const T& x, const U& y) {
    return x == y;
  }
};

template <class T>
class lhs_cmp<T, true, false> {
public:
  template <class U>
  bool operator()(const T& x, const U& y) {
    using rt = decltype(x - y);
    return std::fabs(x - y) <= std::numeric_limits<rt>::epsilon();
  }
};

template <class T>
class lhs_cmp<T, false, true> {
public:
  template <class U>
  bool operator()(const T& x, const U& y) {
    return x == static_cast<T>(y);
  }
};

template <class T>
struct lhs {
public:
  lhs(test* parent, const char *file, size_t line, const char *expr,
      bool should_fail, const T& x)
    : test_(parent),
      filename_(file),
      line_(line),
      expr_(expr),
      should_fail_(should_fail),
      value_(x) {
  }

  lhs(const lhs&) = default;

  ~lhs() {
    if (evaluated_) {
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
    typename std::conditional<
      std::is_convertible<U, T>::value,
      T,
      U
    >::type;

  explicit operator bool() {
    evaluated_ = true;
    return static_cast<bool>(value_) ? pass() : fail_unary();
  }

  // pass-or-fail
  template <class U>
  bool pof(bool res, const U& x) {
    evaluated_ = true;
    return res ? pass() : fail(x);
  }

  template <class U>
  bool operator==(const U& x) {
    lhs_cmp<T> cmp;
    return pof(cmp(value_, x), x);
  }

  template <class U>
  bool operator!=(const U& x) {
    lhs_cmp<T> cmp;
    return pof(! cmp(value_, x), x);
  }

  template <class U>
  bool operator<(const U& x) {
    return pof(value_ < static_cast<elevated<U>>(x), x);
  }

  template <class U>
  bool operator<=(const U& x) {
    return pof(value_ <= static_cast<elevated<U>>(x), x);
  }

  template <class U>
  bool operator>(const U& x) {
    return pof(value_ > static_cast<elevated<U>>(x), x);
  }

  template <class U>
  bool operator>=(const U& x) {
    return pof(value_ >= static_cast<elevated<U>>(x), x);
  }

private:
  template<class V = T>
  typename std::enable_if<
    std::is_convertible<V, bool>::value
    && ! std::is_floating_point<V>::value,
    bool
  >::type
  eval(int) {
    return static_cast<bool>(value_);
  }

  template<class V = T>
  typename std::enable_if<
    std::is_floating_point<V>::value,
    bool
  >::type
  eval(int) {
    return std::fabs(value_) <= std::numeric_limits<V>::epsilon();
  }

  bool eval(long) {
    return true;
  }

  bool pass() {
    passed_ = true;
    std::stringstream ss;
    ss << engine::color(green) << "** "
       << engine::color(blue) << filename_ << engine::color(yellow) << ":"
       << engine::color(blue) << line_ << fill(line_) << engine::color(reset)
       << expr_;
    test_->pass(ss.str());
    return true;
  }

  bool fail_unary() {
    std::stringstream ss;
    ss << engine::color(red) << "!! "
       << engine::color(blue) << filename_ << engine::color(yellow) << ":"
       << engine::color(blue) << line_ << fill(line_) << engine::color(reset)
       << expr_;
    test_->fail(ss.str(), should_fail_);
    return false;
  }

  template <class U>
  bool fail(const U& u) {
    std::stringstream ss;
    ss << engine::color(red) << "!! "
       << engine::color(blue) << filename_ << engine::color(yellow) << ":"
       << engine::color(blue) << line_ << fill(line_) << engine::color(reset)
       << expr_ << engine::color(magenta) << " ("
       << engine::color(red) << show(value_) << engine::color(magenta)
       << " !! " << engine::color(red) << show(u) << engine::color(magenta)
       << ')' << engine::color(reset);
    test_->fail(ss.str(), should_fail_);
    return false;
  }

  bool evaluated_ = false;
  bool passed_ = false;
  test* test_;
  const char* filename_;
  size_t line_;
  const char* expr_;
  bool should_fail_;
  const T& value_;
};

struct expr {
public:
  expr(test* parent, const char* filename, size_t lineno, bool should_fail,
       const char* expression);

  template <class T>
  lhs<T> operator->*(const T& x) {
    return {test_, filename_, line_, expr_, should_fail_, x};
  }

private:
  test* test_;
  const char* filename_;
  size_t line_;
  bool should_fail_;
  const char* expr_;
};

} // namespace detail
} // namespace test
} // namespace caf

// on the global namespace so that it can hidden via namespace-scoping
using caf_test_case_auto_fixture = caf::test::dummy_fixture;

#define CAF_TEST_PR(level, msg, colorcode)                                     \
  ::caf::test::logger::instance(). level ()                                    \
    << ::caf::test::engine::color(::caf::test:: colorcode )                    \
    << "  -> " << ::caf::test::engine::color(::caf::test::reset) << msg << '\n'

#define CAF_TEST_ERROR(msg)                                                    \
  CAF_TEST_PR(info, msg, red)

#define CAF_TEST_INFO(msg)                                                     \
  CAF_TEST_PR(info, msg, yellow)

#define CAF_TEST_VERBOSE(msg)                                                  \
  CAF_TEST_PR(verbose, msg, yellow)

#define CAF_PASTE_CONCAT(lhs, rhs) lhs ## rhs

#define CAF_PASTE(lhs, rhs) CAF_PASTE_CONCAT(lhs, rhs)

#define CAF_UNIQUE(name) CAF_PASTE(name, __LINE__)

#ifndef CAF_SUITE
#define CAF_SUITE unnamed
#endif

#define CAF_STR(s) #s

#define CAF_XSTR(s) CAF_STR(s)

#define CAF_CHECK(...)                                                         \
  do {                                                                         \
    static_cast<void>(::caf::test::detail::expr{                               \
             ::caf::test::engine::current_test(), __FILE__, __LINE__,          \
             false, #__VA_ARGS__} ->* __VA_ARGS__);                            \
    ::caf::test::engine::last_check_file(__FILE__);                            \
    ::caf::test::engine::last_check_line(__LINE__);                            \
  } while(false)

#define CAF_CHECK_FAIL(...)                                                    \
   do {                                                                        \
    (void)(::caf::test::detail::expr{                                          \
             ::caf::test::engine::current_test(), __FILE__, __LINE__,          \
             true, #__VA_ARGS__} ->* __VA_ARGS__);                             \
    ::caf::test::engine::last_check_file(__FILE__);                            \
    ::caf::test::engine::last_check_line(__LINE__);                            \
  } while(false)


#define CAF_FAIL(msg)                                                          \
  do {                                                                         \
    CAF_TEST_ERROR(msg);                                                       \
    throw ::caf::test::detail::require_error{"test failure"};                  \
  } while(false)

#define CAF_REQUIRE(...)                                                       \
  do {                                                                         \
    auto CAF_UNIQUE(__result) =                                                \
      ::caf::test::detail::expr{::caf::test::engine::current_test(),           \
      __FILE__, __LINE__, false, #__VA_ARGS__} ->* __VA_ARGS__;                \
    if (! CAF_UNIQUE(__result)) {                                              \
      throw ::caf::test::detail::require_error{#__VA_ARGS__};                  \
    }                                                                          \
    ::caf::test::engine::last_check_file(__FILE__);                            \
    ::caf::test::engine::last_check_line(__LINE__);                            \
  } while(false)

#define CAF_TEST(name)                                                         \
  namespace {                                                                  \
  struct CAF_UNIQUE(test) : caf_test_case_auto_fixture {                       \
    void run();                                                                \
  };                                                                           \
  ::caf::test::detail::adder< ::caf::test::test_impl<CAF_UNIQUE(test)>>        \
  CAF_UNIQUE(a) {CAF_XSTR(CAF_SUITE), CAF_XSTR(name)};                         \
  } /* namespace <anonymous> */                                                \
  void CAF_UNIQUE(test)::run()

#define CAF_TEST_FIXTURE_SCOPE(scope_name, fixture_name)                       \
  namespace scope_name { using caf_test_case_auto_fixture = fixture_name ;

#define CAF_TEST_FIXTURE_SCOPE_END()                                           \
  } // namespace <scope_name>

// Boost Test compatibility macro
#define CAF_CHECK_EQUAL(x, y) CAF_CHECK(x == y)
#define CAF_MESSAGE(msg) CAF_TEST_VERBOSE(msg)

#endif // CAF_TEST_UNIT_TEST_HPP
