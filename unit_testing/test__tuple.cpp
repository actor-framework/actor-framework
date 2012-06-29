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
#include "cppa/cow_tuple.hpp"
#include "cppa/pattern.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/to_string.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/tpartial_function.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/rm_option.hpp"
#include "cppa/util/purge_refs.hpp"
#include "cppa/util/deduce_ref_type.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/projection.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/value_guard.hpp"
#include "cppa/detail/object_array.hpp"

#include <boost/progress.hpp>

using std::cout;
using std::endl;

using namespace cppa;
using namespace cppa::detail;



#define VERBOSE(LineOfCode) cout << #LineOfCode << " = " << (LineOfCode) << endl


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

typedef util::type_list<int, int, int, float, int, float, float> zz0;

typedef util::type_list<util::type_list<int, int, int>,
                        util::type_list<float>,
                        util::type_list<int>,
                        util::type_list<float, float> > zz8;

typedef util::type_list<
            util::type_list<
                util::type_pair<std::integral_constant<size_t,0>, int>,
                util::type_pair<std::integral_constant<size_t,1>, int>,
                util::type_pair<std::integral_constant<size_t,2>, int>
            >,
            util::type_list<
                util::type_pair<std::integral_constant<size_t,3>, float>
            >,
            util::type_list<
                util::type_pair<std::integral_constant<size_t,4>, int>
            >,
            util::type_list<
                util::type_pair<std::integral_constant<size_t,5>, float>,
                util::type_pair<std::integral_constant<size_t,6>, float>
            >
        >
        zz9;


template<typename First, typename Second>
struct is_same_ : std::is_same<typename First::second, typename Second::second> {
};

#define CPPA_CHECK_INVOKED(FunName, Args)                                      \
    if ( ( FunName Args ) == false || invoked != #FunName ) {                  \
        CPPA_ERROR("invocation of " #FunName " failed");                       \
    } invoked = ""

#define CPPA_CHECK_NOT_INVOKED(FunName, Args)                                  \
    if ( ( FunName Args ) == true || invoked == #FunName ) {                   \
        CPPA_ERROR(#FunName " erroneously invoked");                           \
    } invoked = ""

size_t test__tuple() {
    CPPA_TEST(test__tuple);

    using namespace cppa::placeholders;

    typedef typename util::tl_group_by<zz0, std::is_same>::type zz1;

    typedef typename util::tl_zip_with_index<zz0>::type zz2;

    static_assert(std::is_same<zz1, zz8>::value, "group_by failed");

    typedef typename util::tl_group_by<zz2, is_same_>::type zz3;

    static_assert(std::is_same<zz3, zz9>::value, "group_by failed");

    typedef util::type_list<int, int> token1;
    typedef util::type_list<float> token2;

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
    CPPA_CHECK_EQUAL("f02", invoked);
    invoked = "";

    auto f03 = on(42, val<int>) >> [&](const int& a, int&) { invoked = "f03"; CPPA_CHECK_EQUAL(42, a); };
    CPPA_CHECK_NOT_INVOKED(f03, (0, 0));
    CPPA_CHECK_INVOKED(f03, (42, 42));

    auto f04 = on(42, int2str).when(_x2 == "42") >> [&](std::string& str) {
        CPPA_CHECK_EQUAL("42", str);
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
    CPPA_CHECK_EQUAL(8, f08_val);
    any_tuple f08_any_val = make_cow_tuple(666);
    CPPA_CHECK(f08.invoke(f08_any_val));
    CPPA_CHECK_EQUAL(8, f08_any_val.get_as<int>(0));

    int f09_val = 666;
    auto f09 = on(str2int, val<int>) >> [&](int& mref) { mref = 9; invoked = "f09"; };
    CPPA_CHECK_NOT_INVOKED(f09, ("hello lambda", f09_val));
    CPPA_CHECK_INVOKED(f09, ("0", f09_val));
    CPPA_CHECK_EQUAL(9, f09_val);
    any_tuple f09_any_val = make_cow_tuple("0", 666);
    CPPA_CHECK(f09.invoke(f09_any_val));
    CPPA_CHECK_EQUAL(9, f09_any_val.get_as<int>(1));
    f09_any_val.get_as_mutable<int>(1) = 666;
    any_tuple f09_any_val_copy{f09_any_val};
    CPPA_CHECK_EQUAL(f09_any_val.at(0), f09_any_val_copy.at(0));
    // detaches f09_any_val from f09_any_val_copy
    CPPA_CHECK(f09.invoke(f09_any_val));
    CPPA_CHECK_EQUAL(9, f09_any_val.get_as<int>(1));
    CPPA_CHECK_EQUAL(666, f09_any_val_copy.get_as<int>(1));
    // no longer the same data
    CPPA_CHECK_NOT_EQUAL(f09_any_val.at(0), f09_any_val_copy.at(0));

    auto f10 = (
        on<int>().when(_x1 < 10)    >> [&]() { invoked = "f10.0"; },
        on<int>()                   >> [&]() { invoked = "f10.1"; },
        on<std::string, anything>() >> [&](std::string&) { invoked = "f10.2"; }
    );

    CPPA_CHECK(f10(9));
    CPPA_CHECK_EQUAL("f10.0", invoked);
    CPPA_CHECK(f10(10));
    CPPA_CHECK_EQUAL("f10.1", invoked);
    CPPA_CHECK(f10("42"));
    CPPA_CHECK_EQUAL("f10.2", invoked);
    CPPA_CHECK(f10("42", 42));
    CPPA_CHECK(f10("a", "b", "c"));
    std::string foobar = "foobar";
    CPPA_CHECK(f10(foobar, "b", "c"));
    CPPA_CHECK(f10("a", static_cast<const std::string&>(foobar), "b", "c"));
    //CPPA_CHECK(f10(const static_cast<std::string&>(foobar), "b", "c"));

    int f11_fun = 0;
    auto f11 = (
        on<int>().when(_x1 == 1) >> [&]() { f11_fun =  1; },
        on<int>().when(_x1 == 2) >> [&]() { f11_fun =  2; },
        on<int>().when(_x1 == 3) >> [&]() { f11_fun =  3; },
        on<int>().when(_x1 == 4) >> [&]() { f11_fun =  4; },
        on<int>().when(_x1 == 5) >> [&]() { f11_fun =  5; },
        on<int>().when(_x1 == 6) >> [&]() { f11_fun =  6; },
        on<int>().when(_x1 == 7) >> [&]() { f11_fun =  7; },
        on<int>().when(_x1 == 8) >> [&]() { f11_fun =  8; },
        on<int>().when(_x1 >= 9) >> [&]() { f11_fun =  9; },
        on(str2int)              >> [&]() { f11_fun = 10; },
        on<std::string>()        >> [&]() { f11_fun = 11; }
    );

    CPPA_CHECK(f11(1));
    CPPA_CHECK_EQUAL(1, f11_fun);
    CPPA_CHECK(f11(3));
    CPPA_CHECK_EQUAL(3, f11_fun);
    CPPA_CHECK(f11(8));
    CPPA_CHECK_EQUAL(8, f11_fun);
    CPPA_CHECK(f11(10));
    CPPA_CHECK_EQUAL(9, f11_fun);
    CPPA_CHECK(f11("hello lambda"));
    CPPA_CHECK_EQUAL(11, f11_fun);
    CPPA_CHECK(f11("10"));
    CPPA_CHECK_EQUAL(10, f11_fun);

    auto f12 = (
        on<int, anything, int>().when(_x1 < _x2) >> [&](int a, int b) {
            CPPA_CHECK_EQUAL(1, a);
            CPPA_CHECK_EQUAL(5, b);
            invoked = "f12";
        }
    );
    CPPA_CHECK_INVOKED(f12, (1, 2, 3, 4, 5));

    int f13_fun = 0;
    auto f13 = (
        on<int, anything, std::string, anything, int>().when(_x1 < _x3 && _x2.starts_with("-")) >> [&](int a, const std::string& str, int b) {
            CPPA_CHECK_EQUAL("-h", str);
            CPPA_CHECK_EQUAL(1, a);
            CPPA_CHECK_EQUAL(10, b);
            f13_fun = 1;
            invoked = "f13";
        },
        on<anything, std::string, anything, int, anything, float, anything>() >> [&](const std::string& str, int a, float b) {
            CPPA_CHECK_EQUAL("h", str);
            CPPA_CHECK_EQUAL(12, a);
            CPPA_CHECK_EQUAL(1.f, b);
            f13_fun = 2;
            invoked = "f13";
        },
        on<float, anything, float>().when(_x1 * 2 == _x2) >> [&](float a, float b) {
            CPPA_CHECK_EQUAL(1.f, a);
            CPPA_CHECK_EQUAL(2.f, b);
            f13_fun = 3;
            invoked = "f13";
        }
    );
    CPPA_CHECK_INVOKED(f13, (1, 2, "-h", 12, 32, 10, 1.f, "--foo", 10));
    CPPA_CHECK_EQUAL(1, f13_fun);
    CPPA_CHECK_INVOKED(f13, (1, 2, "h", 12, 32, 10, 1.f, "--foo", 10));
    CPPA_CHECK_EQUAL(2, f13_fun);
    CPPA_CHECK_INVOKED(f13, (1.f, 1.5f, 2.f));
    CPPA_CHECK_EQUAL(3, f13_fun);

    //exit(0);

    return CPPA_TEST_RESULT;

    auto old_pf = (
        on(42) >> []() { },
        on("abc") >> []() { },
        on<int, int>() >> []() { },
        on<anything>() >> []() { }
    );

    auto new_pf = (
        on(42) >> []() { },
        on(std::string("abc")) >> []() { },
        on<int, int>() >> []() { },
        on<anything>() >> []() { }
    );

    any_tuple testee[] = {
        make_cow_tuple(42),
        make_cow_tuple("abc"),
        make_cow_tuple("42"),
        make_cow_tuple(1, 2),
        make_cow_tuple(1, 2, 3)
    };

    constexpr size_t numInvokes = 100000000;

    /*
    auto xvals = make_cow_tuple(1, 2, "3");

    std::string three{"3"};

    std::atomic<std::string*> p3{&three};

    auto guard1 = _x1 == 1;
    auto guard2 = _x1 + _x2 == 3;
    auto guard3 = _x1 + _x2 == 3 && _x3 == "3";

    int dummy_counter = 0;

    cout << "time for for " << numInvokes << " guards(1)*" << endl; {
        boost::progress_timer t0;
        for (size_t i = 0; i < numInvokes; ++i)
            if (guard1(1, 2, *p3))//const_cast<std::string&>(three)))
                ++dummy_counter;
    }

    cout << "time for for " << numInvokes << " guards(1)*" << endl; {
        boost::progress_timer t0;
        for (size_t i = 0; i < numInvokes; ++i)
            if (guard1(get_ref<0>(xvals), get_ref<1>(xvals), get_ref<2>(xvals)))
                ++dummy_counter;
    }

    cout << "time for for " << numInvokes << " guards(1)" << endl; {
        boost::progress_timer t0;
        for (size_t i = 0; i < numInvokes; ++i)
            if (util::unchecked_apply_tuple<bool>(guard1, xvals))
                ++dummy_counter;
    }

    cout << "time for for " << numInvokes << " guards(2)" << endl; {
        boost::progress_timer t0;
        for (size_t i = 0; i < numInvokes; ++i)
            if (util::unchecked_apply_tuple<bool>(guard2, xvals))
                ++dummy_counter;
    }

    cout << "time for for " << numInvokes << " guards(3)" << endl; {
        boost::progress_timer t0;
        for (size_t i = 0; i < numInvokes; ++i)
            if (util::unchecked_apply_tuple<bool>(guard3, xvals))
                ++dummy_counter;
    }

    cout << "time for " << numInvokes << " equal if-statements" << endl; {
        boost::progress_timer t0;
        for (size_t i = 0; i < (numInvokes / sizeof(testee)); ++i) {
            if (get<0>(xvals) + get<1>(xvals) == 3 && get<2>(xvals) == "3") {
                ++dummy_counter;
            }
        }
    }
    */

    cout << "old partial function implementation for " << numInvokes << " matches" << endl; {
        boost::progress_timer t0;
        for (size_t i = 0; i < (numInvokes / sizeof(testee)); ++i) {
            for (auto& x : testee) { old_pf(x); }
        }
    }

    cout << "new partial function implementation for " << numInvokes << " matches" << endl; {
        boost::progress_timer t0;
        for (size_t i = 0; i < (numInvokes / sizeof(testee)); ++i) {
            for (auto& x : testee) { new_pf.invoke(x); }
        }
    }

    cout << "old partial function with on() inside loop" << endl; {
        boost::progress_timer t0;
        for (size_t i = 0; i < (numInvokes / sizeof(testee)); ++i) {
            auto tmp = (
                on(42) >> []() { },
                on("abc") >> []() { },
                on<int, int>() >> []() { },
                on<anything>() >> []() { }
            );
            for (auto& x : testee) { tmp(x); }
        }
    }

    cout << "new partial function with on() inside loop" << endl; {
        boost::progress_timer t0;
        for (size_t i = 0; i < (numInvokes / sizeof(testee)); ++i) {
            auto tmp = (
                on(42) >> []() { },
                on(std::string("abc")) >> []() { },
                on<int, int>() >> []() { },
                on<anything>() >> []() { }
            );
            for (auto& x : testee) { tmp(x); }
        }
    }

    exit(0);

    /*
    VERBOSE(f00(42, 42));
    VERBOSE(f01(42, 42));
    VERBOSE(f02(42, 42));
    VERBOSE(f02(42, 21));
    VERBOSE(f03(42, 42));

    cout << detail::demangle(typeid(f04).name()) << endl;

    VERBOSE(f04(42, 42));
    VERBOSE(f04(42, std::string("42")));

    VERBOSE(f05(42, 42));
    VERBOSE(f05(42, std::string("41")));
    VERBOSE(f05(42, std::string("42")));
    VERBOSE(f05(42, std::string("hello world!")));

    auto f06 = f04.or_else(on<int, int>().when(_x2 > _x1) >> []() { });
    VERBOSE(f06(42, 42));
    VERBOSE(f06(1, 2));
    */

    /*
    auto f06 = on<anything, int>() >> []() { };

    VERBOSE(f06(1));
    VERBOSE(f06(1.f, 2));
*/

    /*

    auto f0 = cfun<token1>([](int, int) { cout << "f0[0]!" << endl; }, _x1 < _x2)
             //.or_else(f00)
             .or_else(cfun<token1>([](int, int) { cout << "f0[1]!" << endl; }, _x1 > _x2))
             .or_else(cfun<token1>([](int, int) { cout << "f0[2]!" << endl; }, _x1 == 2 && _x2 == 2))
             .or_else(cfun<token2>([](float) { cout << "f0[3]!" << endl; }, value_guard< util::type_list<> >{}))
             .or_else(cfun<token1>([](int, int) { cout << "f0[4]" << endl; }, value_guard< util::type_list<> >{}));

    //VERBOSE(f0(make_cow_tuple(1, 2)));

    VERBOSE(f0(3, 3));
    VERBOSE(f0.invoke(make_cow_tuple(3, 3)));

    VERBOSE(f0.invoke(make_cow_tuple(2, 2)));
    VERBOSE(f0.invoke(make_cow_tuple(3, 2)));
    VERBOSE(f0.invoke(make_cow_tuple(1.f)));

    auto f1 = cfun<token1>([](float, int) { cout << "f1!" << endl; }, _x1 < 6, tofloat);

    VERBOSE(f1.invoke(make_cow_tuple(5, 6)));
    VERBOSE(f1.invoke(make_cow_tuple(6, 7)));

    auto i2 = make_cow_tuple(1, 2);

    VERBOSE(f0.invoke(*i2.vals()->type_token(), i2.vals()->impl_type(), i2.vals()->native_data(), *(i2.vals())));
    VERBOSE(f1.invoke(*i2.vals()->type_token(), i2.vals()->impl_type(), i2.vals()->native_data(), *(i2.vals())));

    any_tuple dt1; {
        auto oarr = new detail::object_array;
        oarr->push_back(object::from(1));
        oarr->push_back(object::from(2));
        dt1 = any_tuple{static_cast<detail::abstract_tuple*>(oarr)};
    }


    VERBOSE(f0.invoke(*dt1.cvals()->type_token(), dt1.cvals()->impl_type(), dt1.cvals()->native_data(), *dt1.cvals()));
    VERBOSE(f1.invoke(*dt1.cvals()->type_token(), dt1.cvals()->impl_type(), dt1.cvals()->native_data(), *dt1.cvals()));

    */

    // check type correctness of make_cow_tuple()
    auto t0 = make_cow_tuple("1", 2);
    CPPA_CHECK((std::is_same<decltype(t0), cppa::cow_tuple<std::string, int>>::value));
    auto t0_0 = get<0>(t0);
    auto t0_1 = get<1>(t0);
    // check implicit type conversion
    CPPA_CHECK((std::is_same<decltype(t0_0), std::string>::value));
    CPPA_CHECK((std::is_same<decltype(t0_1), int>::value));
    CPPA_CHECK_EQUAL(t0_0, "1");
    CPPA_CHECK_EQUAL(t0_1, 2);
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
        CPPA_CHECK_EQUAL(&get<0>(t0), &get<0>(v0));     // point to the same
        get_ref<0>(t0) = "hello world";                 // detaches t0 from v0
        CPPA_CHECK_EQUAL(get<0>(t0), "hello world");    // t0 contains new value
        CPPA_CHECK_EQUAL(get<0>(v0), "1");              // v0 contains old value
        CPPA_CHECK_NOT_EQUAL(&get<0>(t0), &get<0>(v0)); // no longer the same
        // check operator==
        auto lhs = make_cow_tuple(1,2,3,4);
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
            CPPA_CHECK_EQUAL(&get<0>(*opt0), at1.at(0));
            CPPA_CHECK_EQUAL(&get<1>(*opt0), at1.at(1));
            CPPA_CHECK_EQUAL(&get<2>(*opt0), at1.at(2));
            CPPA_CHECK_EQUAL(&get<3>(*opt0), at1.at(3));
        }
        // leading wildcard
        auto opt1 = tuple_cast<anything, double>(at1);
        CPPA_CHECK(opt1);
        if (opt1) {
            CPPA_CHECK_EQUAL(get<0>(*opt1), 4.0);
            CPPA_CHECK_EQUAL(&get<0>(*opt1), at1.at(3));
        }
        // trailing wildcard
        auto opt2 = tuple_cast<std::string, anything>(at1);
        CPPA_CHECK(opt2);
        if (opt2) {
            CPPA_CHECK_EQUAL(get<0>(*opt2), "one");
            CPPA_CHECK_EQUAL(&get<0>(*opt2), at1.at(0));
        }
        // wildcard in between
        auto opt3 = tuple_cast<std::string, anything, double>(at1);
        CPPA_CHECK(opt3);
        if (opt3) {
            CPPA_CHECK((*opt3 == make_cow_tuple("one", 4.0)));
            CPPA_CHECK_EQUAL(get<0>(*opt3), "one");
            CPPA_CHECK_EQUAL(get<1>(*opt3), 4.0);
            CPPA_CHECK_EQUAL(&get<0>(*opt3), at1.at(0));
            CPPA_CHECK_EQUAL(&get<1>(*opt3), at1.at(3));
        }
    }
    return CPPA_TEST_RESULT;
}
