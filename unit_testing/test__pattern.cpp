#include <string>
#include <sstream>
#include <functional>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/atom.hpp"
#include "cppa/match.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/option.hpp"
#include "cppa/pattern.hpp"
#include "cppa/announce.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/partial_function.hpp"

#include "cppa/util/apply_tuple.hpp"
#include "cppa/util/is_primitive.hpp"
#include "cppa/util/is_mutable_ref.hpp"

#include "cppa/detail/types_array.hpp"
#include "cppa/detail/decorated_tuple.hpp"

#include <boost/progress.hpp>

using std::string;
using namespace cppa;

template<typename Arr>
string plot(const Arr& arr) {
    std::ostringstream oss;
    oss << "{ ";
    for (size_t i = 0; i < Arr::size; ++i) {
        if (i > 0) oss << ", ";
        oss << "arr[" << i << "] = " << ((arr[i]) ? arr[i]->name() : "anything");
    }
    oss << " }";
    return oss.str();
}

typedef std::pair<std::int32_t, std::int32_t> foobar;

static detail::types_array<int, anything, float> arr1;
static detail::types_array<int, anything, foobar> arr2;

template<typename T>
void match_test(const T& value) {
    match(value) (
        on<int, int, int, anything>() >> [&](int a, int b, int c) {
            cout << "(" << a << ", " << b << ", " << c << ")" << endl;
        },
        on_arg_match >> [&](const string& str) {
            cout << str << endl;
        }
    );
}

template<class Testee>
void invoke_test(std::vector<any_tuple>& test_tuples, Testee& x) {
    boost::progress_timer t;
    for (int i = 0; i < 1000000; ++i) {
        for (auto& t : test_tuples) x(t);
    }
}

size_t test__pattern() {
    CPPA_TEST(test__pattern);

    pattern<int, anything, int> i3;
    any_tuple i3_any_tup = make_cow_tuple(1, 2, 3);

    announce<foobar>(&foobar::first, &foobar::second);

    static constexpr const char* arr1_as_string =
            "{ arr[0] = @i32, arr[1] = anything, arr[2] = float }";
    CPPA_CHECK_EQUAL(arr1_as_string, plot(arr1));
    static constexpr const char* arr2_as_string =
            "{ arr[0] = @i32, arr[1] = anything, "
            "arr[2] = std::pair<@i32,@i32> }";
    CPPA_CHECK_EQUAL(arr2_as_string, plot(arr2));

    // some pattern objects to play with
    pattern<atom_value, int, string> p0{util::wrapped<atom_value>{}};
    pattern<atom_value, int, string> p1{atom("FooBar")};
    pattern<atom_value, int, string> p2{atom("FooBar"), 42};
    pattern<atom_value, int, string> p3{atom("FooBar"), 42, "hello world"};
    pattern<atom_value, anything, string> p4{atom("FooBar"), anything(), "hello world"};
    pattern<atom_value, anything> p5{atom("FooBar")};
    pattern<anything> p6;
    pattern<atom_value, anything> p7;
    pattern<anything, string> p8;
    pattern<atom_value, int, string> p9{mk_tdata(atom("FooBar"), util::wrapped<int>(), "hello world")};

    pattern<string, string, string> p10{"a", util::wrapped<string>{}, "c"};

    // p0-p9 should accept t0
    any_tuple t0 = make_cow_tuple(atom("FooBar"), 42, "hello world");
    CPPA_CHECK((detail::matches(t0, p0)));
    CPPA_CHECK((detail::matches(t0, p1)));
    CPPA_CHECK((detail::matches(t0, p2)));
    CPPA_CHECK((detail::matches(t0, p3)));
    CPPA_CHECK((detail::matches(t0, p4)));
    CPPA_CHECK((detail::matches(t0, p5)));
    CPPA_CHECK((detail::matches(t0, p6)));
    CPPA_CHECK((detail::matches(t0, p7)));
    CPPA_CHECK((detail::matches(t0, p8)));
    CPPA_CHECK((detail::matches(t0, p9)));

    CPPA_CHECK(p0._matches_values(t0));
    CPPA_CHECK(p1._matches_values(t0));
    CPPA_CHECK(p2._matches_values(t0));
    CPPA_CHECK(p3._matches_values(t0));
    CPPA_CHECK(p4._matches_values(t0));
    CPPA_CHECK(p5._matches_values(t0));
    CPPA_CHECK(p6._matches_values(t0));
    CPPA_CHECK(p7._matches_values(t0));
    CPPA_CHECK(p8._matches_values(t0));
    CPPA_CHECK(p9._matches_values(t0));

    any_tuple t1 = make_cow_tuple("a", "b", "c");
    CPPA_CHECK((detail::matches(t1, p8)));
    CPPA_CHECK(p8._matches_values(t1));
    CPPA_CHECK((detail::matches(t1, p10)));
    CPPA_CHECK(p10._matches_values(t1));

    std::vector<string> vec{"a", "b", "c"};
    any_tuple t2 = any_tuple::view(vec);
    CPPA_CHECK((detail::matches(t2, p8)));
    CPPA_CHECK(p8._matches_values(t2));
    CPPA_CHECK((detail::matches(t2, p10)));
    CPPA_CHECK(p10._matches_values(t2));

    pattern<atom_value, int> p11{atom("foo")};
    any_tuple t3 = make_cow_tuple(atom("foo"), 42);
    CPPA_CHECK((detail::matches(t3, p11)));
    CPPA_CHECK(p11._matches_values(t3));

    bool invoked = false;
    match(t3) (
        on<atom("foo"), int>() >> [&](int i) {
            invoked = true;
            CPPA_CHECK_EQUAL(42, i);
        }
    );
    CPPA_CHECK_EQUAL(true, invoked);

    pattern<char, short, int, long> p12;
    pattern<char, short, int, long> p13{char(0), short(1), int(2), long(3)};
    any_tuple t4 = make_cow_tuple(char(0), short(1), int(2), long(3));
    CPPA_CHECK((detail::matches(t4, p12)));
    CPPA_CHECK(p12._matches_values(t4));
    CPPA_CHECK((detail::matches(t4, p13)));
    CPPA_CHECK(p13._matches_values(t4));

    invoked = false;
    match('a') (
        on<char>() >> [&](char c) {
            invoked = true;
            CPPA_CHECK_EQUAL('a', c);
        }
    );
    CPPA_CHECK_EQUAL(true, invoked);

    invoked = false;
    const char muhaha = 'a';
    match(muhaha) (
        on<char>() >> [&](char c) {
            invoked = true;
            CPPA_CHECK_EQUAL('a', c);
        }
    );
    CPPA_CHECK_EQUAL(true, invoked);

    /*
    CPPA_CHECK(p0(x));
    CPPA_CHECK(p1(x));
    CPPA_CHECK(p2(x));
    CPPA_CHECK(p3(x));
    CPPA_CHECK(p4(x));
    CPPA_CHECK(p5(x));
    CPPA_CHECK(p6(x));
    CPPA_CHECK(p7(x));
    CPPA_CHECK(p8(x));
    // ... but p2 and p3 should reject y
    auto y = make_cow_tuple(atom("FooBar"), 24, "hello world");
    CPPA_CHECK_EQUAL(p0(y), true);
    CPPA_CHECK_EQUAL(p1(y), true);
    CPPA_CHECK_EQUAL(p2(y), false);
    CPPA_CHECK_EQUAL(p3(y), false);
    CPPA_CHECK_EQUAL(p4(y), true);
    CPPA_CHECK_EQUAL(p5(y), true);
    CPPA_CHECK_EQUAL(p6(y), true);
    CPPA_CHECK_EQUAL(p7(y), true);
    CPPA_CHECK_EQUAL(p8(y), true);
    // let's check some invoke rules
    constexpr size_t num_lambdas = 6;
    bool lambda_invoked[num_lambdas];
    auto reset_invoke_states = [&]() {
        for (size_t i = 0; i < num_lambdas; ++i) {
            lambda_invoked[i] = false;
        }
    };
    reset_invoke_states();
    auto patterns = (
        on<int, anything, int>() >> [&](int v1, int v2) {
            CPPA_CHECK_EQUAL(v1, 1);
            CPPA_CHECK_EQUAL(v2, 3);
            lambda_invoked[0] = true;
        },
        on<string>() >> [&](const string& str) {
            CPPA_CHECK_EQUAL(str, "hello foo");
            lambda_invoked[1] = true;
        },
        on("1", val<int>, any_vals) >> [&](int value) {
            CPPA_CHECK_EQUAL(value, 2);
            lambda_invoked[2] = true;
        },
        on(1, val<string>(), any_vals) >> [&](const string& str) {
            CPPA_CHECK_EQUAL(str, "2");
            lambda_invoked[3] = true;
        },
        on<atom("Foo"), int>() >> [&](int value) {
            CPPA_CHECK_EQUAL(value, 1);
            lambda_invoked[4] = true;
        },
        on_arg_match >> [&](double v1, const float& v2) {
            CPPA_CHECK_EQUAL(v1, 1.0);
            CPPA_CHECK_EQUAL(v2, 2.0f);
            lambda_invoked[5] = true;
        }
    );
    // invokes lambda 0
    patterns(make_cow_tuple(1, "2", 3));
    CPPA_CHECK(lambda_invoked[0]);
    reset_invoke_states();
    // invokes lambda 1
    patterns(make_cow_tuple("hello foo"));
    CPPA_CHECK(lambda_invoked[1]);
    reset_invoke_states();
    // invokes lambda 2
    patterns(make_cow_tuple("1", 2, 3));
    CPPA_CHECK(lambda_invoked[2]);
    reset_invoke_states();
    // invokes lambda 3
    patterns(make_cow_tuple(1, "2", "3"));
    CPPA_CHECK(lambda_invoked[3]);
    reset_invoke_states();
    // invokes lambda 4
    patterns(make_cow_tuple(atom("Foo"), 1));
    CPPA_CHECK(lambda_invoked[4]);
    reset_invoke_states();
    // invokes lambda 5
    patterns(make_cow_tuple(1.0, 2.0f));
    CPPA_CHECK(lambda_invoked[5]);
    reset_invoke_states();

    */
    return CPPA_TEST_RESULT;
}
