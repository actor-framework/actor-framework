#ifndef TEST_HPP
#define TEST_HPP

#include <vector>
#include <string>
#include <cstring>
#include <cstddef>
#include <sstream>
#include <iostream>
#include <type_traits>

#include "caf/all.hpp"
#include "caf/actor.hpp"
#include "caf/config.hpp"
#include "caf/shutdown.hpp"
#include "caf/to_string.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/scope_guard.hpp"

#ifndef CAF_WINDOWS
constexpr char to_dev_null[] = " &>/dev/null";
#else
constexpr char to_dev_null[] = "";
#endif // CAF_WINDOWS

void set_default_test_settings();

size_t caf_error_count();
void caf_inc_error_count();
std::string caf_fill4(size_t value);
const char* caf_strip_path(const char* file);
void caf_unexpected_message(const char* file, size_t line, caf::message t);
void caf_unexpected_timeout(const char* file, size_t line);

#define CAF_STREAMIFY(fname, line, message)                   \
  caf_strip_path(fname) << ":" << caf_fill4(line) << " " << message

#define CAF_PRINTC(filename, linenum, message)                \
  CAF_LOGF_INFO(CAF_STREAMIFY(filename, linenum, message));        \
  std::cout << CAF_STREAMIFY(filename, linenum, message) << std::endl

#define CAF_PRINT(message) CAF_PRINTC(__FILE__, __LINE__, message)

#if CAF_LOG_LEVEL > 1
#define CAF_PRINTERRC(fname, linenum, msg)                  \
  CAF_LOGF_ERROR(CAF_STREAMIFY(fname, linenum, msg));            \
  std::cerr << "ERROR: " << CAF_STREAMIFY(fname, linenum, msg) << std::endl
#else
#define CAF_PRINTERRC(fname, linenum, msg)                  \
  std::cerr << "ERROR: " << CAF_STREAMIFY(fname, linenum, msg) << std::endl
#endif

#define CAF_PRINTERR(message) CAF_PRINTERRC(__FILE__, __LINE__, message)

template <class T1, typename T2>
struct both_integral {
  static constexpr bool value =
    std::is_integral<T1>::value && std::is_integral<T2>::value;

};

template <bool V, typename T1, typename T2>
struct enable_integral : std::enable_if<both_integral<T1, T2>::value ==
                    V&& not std::is_pointer<T1>::value&& not
                      std::is_pointer<T2>::value> {};

template <class T>
const T& caf_stream_arg(const T& value) {
  return value;
}

inline std::string caf_stream_arg(const caf::actor& ptr) {
  return caf::to_string(ptr);
}

inline std::string caf_stream_arg(const caf::actor_addr& ptr) {
  return caf::to_string(ptr);
}

inline std::string caf_stream_arg(const bool& value) {
  return value ? "true" : "false";
}

inline void caf_passed(const char* fname, size_t line_number) {
  CAF_PRINTC(fname, line_number, "passed");
}

template <class V1, typename V2>
inline void caf_failed(const V1& v1, const V2& v2, const char* fname,
            size_t line_number) {
  CAF_PRINTERRC(fname, line_number,
           "expected value: " << caf_stream_arg(v2)
                    << ", found: " << caf_stream_arg(v1));
  caf_inc_error_count();
}

inline void caf_check_value(const std::string& v1, const std::string& v2,
               const char* fname, size_t line,
               bool expected = true) {
  if ((v1 == v2) == expected)
    caf_passed(fname, line);
  else
    caf_failed(v1, v2, fname, line);
}

template <class V1, typename V2>
inline void caf_check_value(const V1& v1, const V2& v2, const char* fname,
               size_t line, bool expected = true,
               typename enable_integral<false, V1, V2>::type* =
                 0) {
  if (caf::detail::safe_equal(v1, v2) == expected)
    caf_passed(fname, line);
  else
    caf_failed(v1, v2, fname, line);
}

template <class V1, typename V2>
inline void caf_check_value(V1 v1, V2 v2, const char* fname, size_t line,
               bool expected = true,
               typename enable_integral<true, V1, V2>::type* =
                 0) {
  if ((v1 == static_cast<V1>(v2)) == expected)
    caf_passed(fname, line);
  else
    caf_failed(v1, v2, fname, line);
}

#define CAF_VERBOSE_EVAL(LineOfCode)                      \
  CAF_PRINT(#LineOfCode << " = " << (LineOfCode));

#define CAF_TEST(testname)                          \
  auto caf_test_scope_guard = ::caf::detail::make_scope_guard([] {     \
    std::cout << caf_error_count() << " error(s) detected" << std::endl;  \
  });                                    \
  set_default_test_settings();                         \
  CAF_LOGF_INFO("run unit test " << #testname)

#define CAF_TEST_RESULT() ((caf_error_count() == 0) ? 0 : -1)

#define CAF_CHECK_VERBOSE(line_of_code, err_stream)               \
  if (!(line_of_code)) {                           \
    std::cerr << err_stream << std::endl;                  \
    caf_inc_error_count();                        \
  } else {                                   \
    CAF_PRINT("passed");                          \
  }                                      \
  ((void)0)

#define CAF_CHECK(line_of_code)                         \
  if (!(line_of_code)) {                           \
    CAF_PRINTERR(#line_of_code);                      \
    caf_inc_error_count();                        \
  } else {                                   \
    CAF_PRINT("passed");                          \
  }                                      \
  CAF_VOID_STMT

#define CAF_CHECK_EQUAL(lhs_loc, rhs_loc)                   \
  caf_check_value((lhs_loc), (rhs_loc), __FILE__, __LINE__)

#define CAF_CHECK_NOT_EQUAL(rhs_loc, lhs_loc)                 \
  caf_check_value((lhs_loc), (rhs_loc), __FILE__, __LINE__, false)

#define CAF_FAILURE(err_msg)                          \
  {                                      \
    CAF_PRINTERR("ERROR: " << err_msg);                   \
    caf_inc_error_count();                        \
  }                                      \
  ((void)0)

#define CAF_CHECKPOINT() CAF_PRINT("passed")

#define CAF_UNEXPECTED_TOUT() caf_unexpected_timeout(__FILE__, __LINE__)

#define CAF_UNEXPECTED_MSG(selfptr)                       \
  caf_unexpected_message(__FILE__, __LINE__, selfptr->last_dequeued())

// some convenience macros for defining callbacks
#define CAF_CHECKPOINT_CB()                           \
  [] { CAF_CHECKPOINT(); }
#define CAF_FAILURE_CB(err_msg)                         \
  [] { CAF_FAILURE(err_msg); }
#define CAF_UNEXPECTED_MSG_CB(selfptr)                    \
  [=] { CAF_UNEXPECTED_MSG(selfptr); }
#define CAF_UNEXPECTED_MSG_CB_REF(selfref)                  \
  [&] { CAF_UNEXPECTED_MSG(selfref); }
#define CAF_UNEXPECTED_TOUT_CB()                        \
  [] { CAF_UNEXPECTED_TOUT(); }

// string projection
template <class T>
caf::optional<T> spro(const std::string& str) {
  T value;
  std::istringstream iss(str);
  if (iss >> value) {
    return value;
  }
  return caf::none;
}

std::vector<std::string> split(const std::string& str, char delim = ' ',
                 bool keep_empties = true);

std::map<std::string, std::string> get_kv_pairs(int argc, char** argv,
                        int begin = 1);

#endif // TEST_HPP
