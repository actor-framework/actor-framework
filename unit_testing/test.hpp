#ifndef TEST_HPP
#define TEST_HPP

#include <vector>
#include <string>
#include <cstring>
#include <cstddef>
#include <sstream>
#include <iostream>
#include <type_traits>

#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/logging.hpp"
#include "cppa/to_string.hpp"
#include "cppa/util/scope_guard.hpp"

void set_default_test_settings();

size_t cppa_error_count();
void cppa_inc_error_count();
std::string cppa_fill4(int value);
const char* cppa_strip_path(const char* fname);
void cppa_unexpected_message(const char* fname, size_t line_num);
void cppa_unexpected_timeout(const char* fname, size_t line_num);

#define CPPA_STREAMIFY(fname, line, message)                                   \
    cppa_strip_path(fname) << ":" << cppa_fill4(line) << " " << message

#define CPPA_PRINTC(filename, linenum, message)                                \
    CPPA_LOGF_INFO(CPPA_STREAMIFY(filename, linenum, message));               \
    std::cout << CPPA_STREAMIFY(filename, linenum, message) << std::endl

#define CPPA_PRINT(message) CPPA_PRINTC(__FILE__, __LINE__, message)

#define CPPA_PRINTERRC(fname, linenum, msg)                                    \
    CPPA_LOGF(CPPA_ERROR, ::cppa::self, CPPA_STREAMIFY(fname, linenum, msg));  \
    std::cerr << "ERROR: " << CPPA_STREAMIFY(fname, linenum, msg)              \
              << std::endl

#define CPPA_PRINTERR(message) CPPA_PRINTERRC(__FILE__, __LINE__, message)

template<typename T1, typename T2>
struct both_integral {
    static constexpr bool value =    std::is_integral<T1>::value
                                  && std::is_integral<T2>::value;
};

template<bool V, typename T1, typename T2>
struct enable_integral : std::enable_if<   both_integral<T1, T2>::value == V
                                        && not std::is_pointer<T1>::value
                                        && not std::is_pointer<T2>::value> { };

template<typename T>
const T& cppa_stream_arg(const T& value) {
    return value;
}

inline std::string cppa_stream_arg(const cppa::actor_ptr& ptr) {
    return cppa::to_string(ptr);
}

inline std::string cppa_stream_arg(const bool& value) {
    return value ? "true" : "false";
}

inline void cppa_passed(const char* fname, int line_number) {
    CPPA_PRINTC(fname, line_number, "passed");
}

template<typename V1, typename V2>
inline void cppa_failed(const V1& v1,
                        const V2& v2,
                        const char* fname,
                        int line_number) {
    CPPA_PRINTERRC(fname, line_number,
                   "expected value: " << cppa_stream_arg(v2)
                   << ", found: " << cppa_stream_arg(v1));
    cppa_inc_error_count();
}

inline void cppa_check_value(const std::string& v1,
                             const std::string& v2,
                             const char* fname,
                             int line,
                             bool expected = true) {
    if ((v1 == v2) == expected) cppa_passed(fname, line);
    else cppa_failed(v1, v2, fname, line);
}

template<typename V1, typename V2>
inline void cppa_check_value(const V1& v1,
                             const V2& v2,
                             const char* fname,
                             int line,
                             bool expected = true,
                             typename enable_integral<false, V1, V2>::type* = 0) {
    if ((v1 == v2) == expected) cppa_passed(fname, line);
    else cppa_failed(v1, v2, fname, line);
}

template<typename V1, typename V2>
inline void cppa_check_value(V1 v1,
                             V2 v2,
                             const char* fname,
                             int line,
                             bool expected = true,
                             typename enable_integral<true, V1, V2>::type* = 0) {
    if ((v1 == static_cast<V1>(v2)) == expected) cppa_passed(fname, line);
    else cppa_failed(v1, v2, fname, line);
}

#define CPPA_VERBOSE_EVAL(LineOfCode)                                          \
    CPPA_PRINT(#LineOfCode << " = " << (LineOfCode));

#define CPPA_TEST(testname)                                                    \
    auto cppa_test_scope_guard = ::cppa::util::make_scope_guard([] {           \
        std::cout << cppa_error_count() << " error(s) detected" << std::endl;  \
    });                                                                        \
    set_default_test_settings();                                               \
    CPPA_LOGF_INFO("run unit test " << #testname)

#define CPPA_TEST_RESULT() ((cppa_error_count() == 0) ? 0 : -1)

#define CPPA_CHECK_VERBOSE(line_of_code, err_stream)                           \
    if (!(line_of_code)) {                                                     \
        std::cerr << err_stream << std::endl;                                  \
        cppa_inc_error_count();                                                \
    }                                                                          \
    else {                                                                     \
        CPPA_PRINT("passed");                                                  \
    } ((void) 0)

#define CPPA_CHECK(line_of_code)                                               \
    if (!(line_of_code)) {                                                     \
        CPPA_PRINTERR(#line_of_code);                                          \
        cppa_inc_error_count();                                                \
    }                                                                          \
    else { CPPA_PRINT("passed"); } CPPA_VOID_STMT

#define CPPA_CHECK_EQUAL(lhs_loc, rhs_loc)                                     \
    cppa_check_value((lhs_loc), (rhs_loc), __FILE__, __LINE__)

#define CPPA_CHECK_NOT_EQUAL(rhs_loc, lhs_loc)                                 \
    cppa_check_value((lhs_loc), (rhs_loc), __FILE__, __LINE__, false)

#define CPPA_FAILURE(err_msg) {                                                  \
        CPPA_PRINTERR("ERROR: " << err_msg);                                   \
        cppa_inc_error_count();                                                \
    } ((void) 0)

#define CPPA_CHECKPOINT()                                                      \
    CPPA_PRINT("passed")

#define CPPA_UNEXPECTED_TOUT()                                                 \
    cppa_unexpected_timeout(__FILE__, __LINE__)

#define CPPA_UNEXPECTED_MSG()                                                  \
    cppa_unexpected_message(__FILE__, __LINE__)

// some convenience macros for defining callbacks
#define CPPA_CHECKPOINT_CB() [] { CPPA_CHECKPOINT(); }
#define CPPA_FAILURE_CB(err_msg) [] { CPPA_FAILURE(err_msg); }
#define CPPA_UNEXPECTED_MSG_CB() [] { CPPA_UNEXPECTED_MSG(); }
#define CPPA_UNEXPECTED_TOUT_CB() [] { CPPA_UNEXPECTED_TOUT(); }

std::vector<std::string> split(const std::string& str, char delim = ' ', bool keep_empties = true);

std::map<std::string, std::string> get_kv_pairs(int argc, char** argv, int begin = 1);

template<typename F>
void run_client_part(const std::map<std::string, std::string>& args, F fun) {
    CPPA_LOGF_INFO("run in client mode");
    auto i = args.find("port");
    if (i == args.end()) {
        CPPA_LOGF_ERROR("no port specified");
        throw std::logic_error("no port specified");
    }
    auto port = static_cast<std::uint16_t>(stoi(i->second));
    fun(port);
    cppa::await_all_others_done();
    cppa::shutdown();
}

#endif // TEST_HPP
