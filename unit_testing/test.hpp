#ifndef TEST_HPP
#define TEST_HPP

#include <vector>
#include <string>
#include <cstddef>
#include <sstream>
#include <iostream>
#include <type_traits>

#include "cppa/util/scope_guard.hpp"

void cppa_inc_error_count();

size_t cppa_error_count();
void cppa_unexpected_message(const char* fname, size_t line_num);
void cppa_unexpected_timeout(const char* fname, size_t line_num);

template<typename T1, typename T2>
inline bool cppa_check_value_fun_eq(const T1& value1, const T2& value2,
                                    typename std::enable_if<
                                           !std::is_integral<T1>::value
                                        || !std::is_integral<T2>::value
                                    >::type* = 0) {
    return value1 == value2;
}

template<typename T1, typename T2>
inline bool cppa_check_value_fun_eq(T1 value1, T2 value2,
                                    typename std::enable_if<
                                           std::is_integral<T1>::value
                                        && std::is_integral<T2>::value
                                    >::type* = 0) {
    return value1 == static_cast<T1>(value2);
}

template<typename T1, typename T2>
inline bool cppa_check_value_fun(const T1& value1, const T2& value2,
                                 const char* file_name,
                                 int line_number) {
    if (cppa_check_value_fun_eq(value1, value2) == false) {
        std::cerr << "ERROR in file " << file_name << " on line " << line_number
                  << " => expected value: " << value1
                  << ", found: " << value2
                  << std::endl;
        cppa_inc_error_count();
        return false;
    }
    return true;
}

template<typename T1, typename T2>
inline void cppa_check_value_verbose_fun(const T1& value1, const T2& value2,
                                         const char* file_name,
                                         int line_number) {
    if (cppa_check_value_fun(value1, value2, file_name, line_number)) {
         std::cout << "line " << line_number << " passed" << std::endl;
    }
}

#define CPPA_TEST(name)                                                        \
    auto cppa_test_scope_guard = ::cppa::util::make_scope_guard([] {           \
        std::cout << cppa_error_count() << " error(s) detected" << std::endl;  \
    });

#define CPPA_TEST_RESULT cppa_error_count()

#define CPPA_IF_VERBOSE(line_of_code) line_of_code
#define CPPA_CHECK(line_of_code)                                               \
if (!(line_of_code)) {                                                         \
    std::cerr << "ERROR in file " << __FILE__ << " on line " << __LINE__       \
              << " => " << #line_of_code << std::endl;                         \
    cppa_inc_error_count();                                                    \
}                                                                              \
else {                                                                         \
    std::cout << "line " << __LINE__ << " passed" << std::endl;                \
} ((void) 0)
#define CPPA_CHECK_EQUAL(lhs_loc, rhs_loc)                                     \
  cppa_check_value_verbose_fun((lhs_loc), (rhs_loc), __FILE__, __LINE__)


#define CPPA_ERROR(err_msg)                                                    \
    std::cerr << "ERROR in file " << __FILE__ << " on line " << __LINE__       \
              << ": " << err_msg << std::endl;                                 \
    cppa_inc_error_count()

#define CPPA_CHECK_NOT_EQUAL(lhs_loc, rhs_loc)                                 \
    CPPA_CHECK(!((lhs_loc) == (rhs_loc)))

#define CPPA_CHECKPOINT()                                                      \
    std::cout << "checkpoint at line " << __LINE__ << " passed" << std::endl

// some convenience macros for defining callbacks
#define CPPA_CHECKPOINT_CB()                                                   \
    [] { CPPA_CHECKPOINT(); }

#define CPPA_UNEXPECTED_MSG_CB()                                               \
    [] { cppa_unexpected_message(__FILE__, __LINE__); }

#define CPPA_UNEXPECTED_TOUT_CB()                                              \
    [] { cppa_unexpected_timeout(__FILE__, __LINE__); }

#define CPPA_ERROR_CB(err_msg)                                                 \
    [] { CPPA_ERROR(err_msg); }

typedef std::pair<std::string, std::string> string_pair;

inline std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream strs{str};
    std::string tmp;
    while (std::getline(strs, tmp, delim)) result.push_back(tmp);
    return result;
}

//using std::cout;
//using std::endl;
//using std::cerr;

#endif // TEST_HPP
