#include <list>
#include <array>
#include <string>
#include <limits>
#include <cstdint>
#include <utility>
#include <iostream>
#include <typeinfo>
#include <functional>
#include <type_traits>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/to_string.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/tpartial_function.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/type_traits.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/projection.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/value_guard.hpp"
#include "cppa/detail/object_array.hpp"

using std::cout;
using std::endl;

using namespace cppa;
using namespace cppa::detail;
using namespace cppa::placeholders;

namespace { std::atomic<size_t> s_expensive_copies; }

struct expensive_copy_struct {

    expensive_copy_struct(const expensive_copy_struct& other) : value(other.value) {
        ++s_expensive_copies;
    }

    expensive_copy_struct(expensive_copy_struct&& other) : value(other.value) { }

    expensive_copy_struct() : value(0) { }

    int value;

};

inline bool operator==(const expensive_copy_struct& lhs,
                       const expensive_copy_struct& rhs) {
    return lhs.value == rhs.value;
}

inline bool operator!=(const expensive_copy_struct& lhs,
                       const expensive_copy_struct& rhs) {
    return !(lhs == rhs);
}

std::string int2str(int i) {
    return std::to_string(i);
}

option<int> str2int(const std::string& str) {
    char* endptr = nullptr;
    int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
    if (endptr != nullptr && *endptr == '\0') {
        return result;
    }
    return {};
}

#define CPPA_CHECK_INVOKED(FunName, Args)                                      \
    if ( ( FunName Args ) == false || invoked != #FunName ) {                  \
        CPPA_FAILURE("invocation of " #FunName " failed");                     \
    } invoked = ""

#define CPPA_CHECK_NOT_INVOKED(FunName, Args)                                  \
    if ( ( FunName Args ) == true || invoked == #FunName ) {                   \
        CPPA_FAILURE(#FunName " erroneously invoked");                         \
    } invoked = ""

struct dummy_receiver : event_based_actor {
    void init() {
        become(
            on_arg_match >> [=](expensive_copy_struct& ecs) {
                ecs.value = 42;
                reply(std::move(ecs));
                quit();
            }
        );
    }
};

template<typename First, typename Second>
struct same_second_type : std::is_same<typename First::second, typename Second::second> { };

void check_type_list() {
    using namespace cppa::util;

    typedef type_list<int, int, int, float, int, float, float> zz0;

    typedef type_list<type_list<int, int, int>,
                      type_list<float>,
                      type_list<int>,
                      type_list<float, float>> zz8;

    typedef type_list<
                type_list<
                    type_pair<std::integral_constant<size_t, 0>, int>,
                    type_pair<std::integral_constant<size_t, 1>, int>,
                    type_pair<std::integral_constant<size_t, 2>, int>
                >,
                type_list<
                    type_pair<std::integral_constant<size_t, 3>, float>
                >,
                type_list<
                    type_pair<std::integral_constant<size_t, 4>, int>
                >,
                type_list<
                    type_pair<std::integral_constant<size_t, 5>, float>,
                    type_pair<std::integral_constant<size_t, 6>, float>
                >
            >
            zz9;

    typedef typename tl_group_by<zz0, std::is_same>::type zz1;

    typedef typename tl_zip_with_index<zz0>::type zz2;

    static_assert(std::is_same<zz1, zz8>::value, "tl_group_by failed");

    typedef typename tl_group_by<zz2, same_second_type>::type zz3;

    static_assert(std::is_same<zz3, zz9>::value, "tl_group_by failed");
}

void check_default_ctors() {
    CPPA_PRINT(__func__);
    cow_tuple<int> zero;
    CPPA_CHECK_EQUAL(get<0>(zero), 0);
}

void check_guards() {
    CPPA_PRINT(__func__);

    std::string invoked;

    auto f00 = on<int, int>() >> [&]() { invoked = "f00"; };
    CPPA_CHECK_INVOKED(f00, (42, 42));

    auto f01 = on<int, int>().when(_x1 == 42) >> [&]() { invoked = "f01"; };
    CPPA_CHECK_INVOKED(f01, (42, 42));
    CPPA_CHECK_NOT_INVOKED(f01, (1, 2));

    auto f02 = on<int, int>().when(_x1 == 42 && _x2 * 2 == _x1) >> [&]() { invoked = "f02"; };
    CPPA_CHECK_NOT_INVOKED(f02, (0, 0));
    CPPA_CHECK_NOT_INVOKED(f02, (42, 42));
    CPPA_CHECK_NOT_INVOKED(f02, (2, 1));
    CPPA_CHECK_INVOKED(f02, (42, 21));

    CPPA_CHECK(f02.invoke(make_cow_tuple(42, 21)));
    CPPA_CHECK_EQUAL(invoked, "f02");
    invoked = "";

    auto f03 = on(42, val<int>) >> [&](const int& a, int&) { invoked = "f03"; CPPA_CHECK_EQUAL(a, 42); };
    CPPA_CHECK_NOT_INVOKED(f03, (0, 0));
    CPPA_CHECK_INVOKED(f03, (42, 42));

    auto f04 = on(42, int2str).when(_x2 == "42") >> [&](std::string& str) {
        CPPA_CHECK_EQUAL(str, "42");
        invoked = "f04";
    };

    CPPA_CHECK_NOT_INVOKED(f04, (0, 0));
    CPPA_CHECK_NOT_INVOKED(f04, (0, 42));
    CPPA_CHECK_NOT_INVOKED(f04, (42, 0));
    CPPA_CHECK_INVOKED(f04, (42, 42));

    auto f05 = on(str2int).when(_x1 % 2 == 0) >> [&]() { invoked = "f05"; };
    CPPA_CHECK_NOT_INVOKED(f05, ("1"));
    CPPA_CHECK_INVOKED(f05, ("2"));

    auto f06 = on(42, str2int).when(_x2 % 2 == 0) >> [&]() { invoked = "f06"; };
    CPPA_CHECK_NOT_INVOKED(f06, (0, "0"));
    CPPA_CHECK_NOT_INVOKED(f06, (42, "1"));
    CPPA_CHECK_INVOKED(f06, (42, "2"));

    int f07_val = 1;
    auto f07 = on<int>().when(_x1 == gref(f07_val)) >> [&]() { invoked = "f07"; };
    CPPA_CHECK_NOT_INVOKED(f07, (0));
    CPPA_CHECK_INVOKED(f07, (1));
    CPPA_CHECK_NOT_INVOKED(f07, (2));
    ++f07_val;
    CPPA_CHECK_NOT_INVOKED(f07, (0));
    CPPA_CHECK_NOT_INVOKED(f07, (1));
    CPPA_CHECK_INVOKED(f07, (2));
    CPPA_CHECK(f07.invoke(make_cow_tuple(2)));

    int f08_val = 666;
    auto f08 = on<int>() >> [&](int& mref) { mref = 8; invoked = "f08"; };
    CPPA_CHECK_INVOKED(f08, (f08_val));
    CPPA_CHECK_EQUAL(f08_val, 8);
    any_tuple f08_any_val = make_cow_tuple(666);
    CPPA_CHECK(f08.invoke(f08_any_val));
    CPPA_CHECK_EQUAL(f08_any_val.get_as<int>(0), 8);

    int f09_val = 666;
    auto f09 = on(str2int, val<int>) >> [&](int& mref) { mref = 9; invoked = "f09"; };
    CPPA_CHECK_NOT_INVOKED(f09, ("hello lambda", f09_val));
    CPPA_CHECK_INVOKED(f09, ("0", f09_val));
    CPPA_CHECK_EQUAL(f09_val, 9);
    any_tuple f09_any_val = make_cow_tuple("0", 666);
    CPPA_CHECK(f09.invoke(f09_any_val));
    CPPA_CHECK_EQUAL(f09_any_val.get_as<int>(1), 9);
    f09_any_val.get_as_mutable<int>(1) = 666;
    any_tuple f09_any_val_copy{f09_any_val};
    CPPA_CHECK(f09_any_val.at(0) == f09_any_val_copy.at(0));
    // detaches f09_any_val from f09_any_val_copy
    CPPA_CHECK(f09.invoke(f09_any_val));
    CPPA_CHECK_EQUAL(f09_any_val.get_as<int>(1), 9);
    CPPA_CHECK_EQUAL(f09_any_val_copy.get_as<int>(1), 666);
    // no longer the same data
    CPPA_CHECK(f09_any_val.at(0) != f09_any_val_copy.at(0));

    auto f10 = (
        on<int>().when(_x1 < 10)    >> [&]() { invoked = "f10.0"; },
        on<int>()                   >> [&]() { invoked = "f10.1"; },
        on<std::string, anything>() >> [&](std::string&) { invoked = "f10.2"; }
    );

    CPPA_CHECK(f10(9));
    CPPA_CHECK_EQUAL(invoked, "f10.0");
    CPPA_CHECK(f10(10));
    CPPA_CHECK_EQUAL(invoked, "f10.1");
    CPPA_CHECK(f10("42"));
    CPPA_CHECK_EQUAL(invoked, "f10.2");
    CPPA_CHECK(f10("42", 42));
    CPPA_CHECK(f10("a", "b", "c"));
    std::string foobar = "foobar";
    CPPA_CHECK(f10(foobar, "b", "c"));
    CPPA_CHECK(f10("a", static_cast<const std::string&>(foobar), "b", "c"));
}

void check_many_cases() {
    CPPA_PRINT(__func__);
    auto on_int = on<int>();

    int f11_fun = 0;
    auto f11 = (
        on_int.when(_x1 == 1) >> [&] { f11_fun =  1; },
        on_int.when(_x1 == 2) >> [&] { f11_fun =  2; },
        on_int.when(_x1 == 3) >> [&] { f11_fun =  3; },
        on_int.when(_x1 == 4) >> [&] { f11_fun =  4; },
        on_int.when(_x1 == 5) >> [&] { f11_fun =  5; },
        on_int.when(_x1 == 6) >> [&] { f11_fun =  6; },
        on_int.when(_x1 == 7) >> [&] { f11_fun =  7; },
        on_int.when(_x1 == 8) >> [&] { f11_fun =  8; },
        on_int.when(_x1 >= 9) >> [&] { f11_fun =  9; },
        on(str2int)           >> [&] { f11_fun = 10; },
        on<std::string>()     >> [&] { f11_fun = 11; }
    );

    CPPA_CHECK(f11(1));
    CPPA_CHECK_EQUAL(f11_fun, 1);
    CPPA_CHECK(f11(3));
    CPPA_CHECK_EQUAL(f11_fun, 3);
    CPPA_CHECK(f11(8));
    CPPA_CHECK_EQUAL(f11_fun, 8);
    CPPA_CHECK(f11(10));
    CPPA_CHECK_EQUAL(f11_fun, 9);
    CPPA_CHECK(f11("hello lambda"));
    CPPA_CHECK_EQUAL(f11_fun, 11);
    CPPA_CHECK(f11("10"));
    CPPA_CHECK_EQUAL(f11_fun, 10);
}

void check_wildcards() {
    CPPA_PRINT(__func__);
    std::string invoked;

    auto f12 = (
        on<int, anything, int>().when(_x1 < _x2) >> [&](int a, int b) {
            CPPA_CHECK_EQUAL(a, 1);
            CPPA_CHECK_EQUAL(b, 5);
            invoked = "f12";
        }
    );
    CPPA_CHECK_INVOKED(f12, (1, 2, 3, 4, 5));

    int f13_fun = 0;
    auto f13 = (
        on<int, anything, std::string, anything, int>().when(_x1 < _x3 && _x2.starts_with("-")) >> [&](int a, const std::string& str, int b) {
            CPPA_CHECK_EQUAL(str, "-h");
            CPPA_CHECK_EQUAL(a, 1);
            CPPA_CHECK_EQUAL(b, 10);
            f13_fun = 1;
            invoked = "f13";
        },
        on<anything, std::string, anything, int, anything, float, anything>() >> [&](const std::string& str, int a, float b) {
            CPPA_CHECK_EQUAL(str, "h");
            CPPA_CHECK_EQUAL(a, 12);
            CPPA_CHECK_EQUAL(b, 1.f);
            f13_fun = 2;
            invoked = "f13";
        },
        on<float, anything, float>().when(_x1 * 2 == _x2) >> [&](float a, float b) {
            CPPA_CHECK_EQUAL(a, 1.f);
            CPPA_CHECK_EQUAL(b, 2.f);
            f13_fun = 3;
            invoked = "f13";
        }
    );
    CPPA_CHECK_INVOKED(f13, (1, 2, "-h", 12, 32, 10, 1.f, "--foo", 10));
    CPPA_CHECK_EQUAL(f13_fun, 1);
    CPPA_CHECK_INVOKED(f13, (1, 2, "h", 12, 32, 10, 1.f, "--foo", 10));
    CPPA_CHECK_EQUAL(f13_fun, 2);
    CPPA_CHECK_INVOKED(f13, (1.f, 1.5f, 2.f));
    CPPA_CHECK_EQUAL(f13_fun, 3);

    // check type correctness of make_cow_tuple()
    auto t0 = make_cow_tuple("1", 2);
    CPPA_CHECK((std::is_same<decltype(t0), cppa::cow_tuple<std::string, int>>::value));
    auto t0_0 = get<0>(t0);
    auto t0_1 = get<1>(t0);
    // check implicit type conversion
    CPPA_CHECK((std::is_same<decltype(t0_0), std::string>::value));
    CPPA_CHECK((std::is_same<decltype(t0_1), int>::value));
    CPPA_CHECK_EQUAL("1", t0_0);
    CPPA_CHECK_EQUAL(2, t0_1);
    // use tuple cast to get a subtuple
    any_tuple at0(t0);
    auto v0opt = tuple_cast<std::string, anything>(at0);
    CPPA_CHECK((std::is_same<decltype(v0opt), option<cow_tuple<std::string>>>::value));
    CPPA_CHECK((v0opt));
    CPPA_CHECK(   at0.size() == 2
               && at0.at(0) == &get<0>(t0)
               && at0.at(1) == &get<1>(t0));
    if (v0opt) {
        auto& v0 = *v0opt;
        CPPA_CHECK((std::is_same<decltype(v0), cow_tuple<std::string>&>::value));
        CPPA_CHECK((std::is_same<decltype(get<0>(v0)), const std::string&>::value));
        CPPA_CHECK_EQUAL(v0.size(), 1);
        CPPA_CHECK_EQUAL(get<0>(v0), "1");
        CPPA_CHECK_EQUAL(get<0>(t0), get<0>(v0));
        // check cow semantics
        CPPA_CHECK(&get<0>(t0) == &get<0>(v0));         // point to the same
        get_ref<0>(t0) = "hello world";                 // detaches t0 from v0
        CPPA_CHECK_EQUAL(get<0>(t0), "hello world");    // t0 contains new value
        CPPA_CHECK_EQUAL(get<0>(v0), "1");              // v0 contains old value
        CPPA_CHECK(&get<0>(t0) != &get<0>(v0));         // no longer the same
        // check operator==
        auto lhs = make_cow_tuple(1, 2, 3, 4);
        auto rhs = make_cow_tuple(static_cast<std::uint8_t>(1), 2.0, 3, 4);
        CPPA_CHECK(lhs == rhs);
        CPPA_CHECK(rhs == lhs);
    }
    any_tuple at1 = make_cow_tuple("one", 2, 3.f, 4.0); {
        // perfect match
        auto opt0 = tuple_cast<std::string, int, float, double>(at1);
        CPPA_CHECK(opt0);
        if (opt0) {
            CPPA_CHECK((*opt0 == make_cow_tuple("one", 2, 3.f, 4.0)));
            CPPA_CHECK(&get<0>(*opt0) == at1.at(0));
            CPPA_CHECK(&get<1>(*opt0) == at1.at(1));
            CPPA_CHECK(&get<2>(*opt0) == at1.at(2));
            CPPA_CHECK(&get<3>(*opt0) == at1.at(3));
        }
        // leading wildcard
        auto opt1 = tuple_cast<anything, double>(at1);
        CPPA_CHECK(opt1);
        if (opt1) {
            CPPA_CHECK_EQUAL(get<0>(*opt1), 4.0);
            CPPA_CHECK(&get<0>(*opt1) == at1.at(3));
        }
        // trailing wildcard
        auto opt2 = tuple_cast<std::string, anything>(at1);
        CPPA_CHECK(opt2);
        if (opt2) {
            CPPA_CHECK_EQUAL(get<0>(*opt2), "one");
            CPPA_CHECK(&get<0>(*opt2) == at1.at(0));
        }
        // wildcard in between
        auto opt3 = tuple_cast<std::string, anything, double>(at1);
        CPPA_CHECK(opt3);
        if (opt3) {
            CPPA_CHECK((*opt3 == make_cow_tuple("one", 4.0)));
            CPPA_CHECK_EQUAL(get<0>(*opt3), "one");
            CPPA_CHECK_EQUAL(get<1>(*opt3), 4.0);
            CPPA_CHECK(&get<0>(*opt3) == at1.at(0));
            CPPA_CHECK(&get<1>(*opt3) == at1.at(3));
        }
    }
}

void check_move_ops() {
    CPPA_PRINT(__func__);
    send(spawn<dummy_receiver>(), expensive_copy_struct());
    receive (
        on_arg_match >> [&](expensive_copy_struct& ecs) {
            CPPA_CHECK_EQUAL(42, ecs.value);
        }
    );
    CPPA_CHECK_EQUAL(s_expensive_copies.load(), 0);
}

int main() {
    CPPA_TEST(test_tuple);
    announce<expensive_copy_struct>(&expensive_copy_struct::value);
    check_type_list();
    check_default_ctors();
    check_guards();
    check_wildcards();
    check_move_ops();
    await_all_others_done();
    shutdown();
    return CPPA_TEST_RESULT();
}
