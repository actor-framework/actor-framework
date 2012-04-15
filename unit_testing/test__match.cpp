#include <functional>

#include "test.hpp"
#include "cppa/on.hpp"
#include "cppa/match.hpp"
#include "cppa/announce.hpp"
#include "cppa/to_string.hpp"
#include "cppa/guard_expr.hpp"

using namespace cppa;

using std::vector;
using std::string;

bool is_even(int i) { return i % 2 == 0; }

/*
 *
 * projection:
 *
 * on(_x.rsplit("--(.*)=(.*)")) >> [](std::vector<std::string> matched_groups)
 *
 */

bool ascending(int a, int b, int c)
{
    return a < b && b < c;
}

size_t test__match()
{
    CPPA_TEST(test__match);

    using namespace std::placeholders;
    using namespace cppa::placeholders;

    auto expr0 = gcall(ascending, _x1, _x2, _x3);
    CPPA_CHECK(ge_invoke(expr0, 1, 2, 3));
    CPPA_CHECK(!ge_invoke(expr0, 3, 2, 1));

    int ival0 = 2;
    auto expr01 = gcall(ascending, _x1, std::ref(ival0), _x2);
    CPPA_CHECK(ge_invoke(expr01, 1, 3));
    ++ival0;
    CPPA_CHECK(!ge_invoke(expr01, 1, 3));

    auto expr1 = _x1 + _x2;
    auto expr2 = _x1 + _x2 < _x3;
    auto expr3 = _x1 % _x2 == 0;
    CPPA_CHECK_EQUAL(5, (ge_invoke(expr1, 2, 3)));
    //CPPA_CHECK(ge_invoke(expr2, 1, 2, 4));
    CPPA_CHECK(expr2(1, 2, 4));
    CPPA_CHECK_EQUAL("12", (ge_invoke(expr1, std::string{"1"}, std::string{"2"})));
    CPPA_CHECK(expr3(100, 2));

    auto expr4 = _x1 == "-h" || _x1 == "--help";
    CPPA_CHECK(ge_invoke(expr4, string("-h")));
    CPPA_CHECK(ge_invoke(expr4, string("--help")));
    CPPA_CHECK(!ge_invoke(expr4, string("-g")));

    auto expr5 = _x1.starts_with("--");
    CPPA_CHECK(ge_invoke(expr5, string("--help")));
    CPPA_CHECK(!ge_invoke(expr5, string("-help")));

    vector<string> vec1{"hello", "world"};
    auto expr6 = _x1.in(vec1);
    CPPA_CHECK(ge_invoke(expr6, string("hello")));
    CPPA_CHECK(ge_invoke(expr6, string("world")));
    CPPA_CHECK(!ge_invoke(expr6, string("hello world")));
    auto expr7 = _x1.in(std::ref(vec1));
    CPPA_CHECK(ge_invoke(expr7, string("hello")));
    CPPA_CHECK(ge_invoke(expr7, string("world")));
    CPPA_CHECK(!ge_invoke(expr7, string("hello world")));
    vec1.emplace_back("hello world");
    CPPA_CHECK(!ge_invoke(expr6, string("hello world")));
    CPPA_CHECK(ge_invoke(expr7, string("hello world")));

    int ival = 5;
    auto expr8 = _x1 == ival;
    auto expr9 = _x1 == std::ref(ival);
    CPPA_CHECK(ge_invoke(expr8, 5));
    CPPA_CHECK(ge_invoke(expr9, 5));
    ival = 10;
    CPPA_CHECK(!ge_invoke(expr9, 5));
    CPPA_CHECK(ge_invoke(expr9, 10));

    auto expr11 = _x1.in({"one", "two"});
    CPPA_CHECK(ge_invoke(expr11, string("one")));
    CPPA_CHECK(ge_invoke(expr11, string("two")));
    CPPA_CHECK(!ge_invoke(expr11, string("three")));

    auto expr12 = _x1 * _x2 < _x3 - _x4;
    CPPA_CHECK(ge_invoke(expr12, 1, 1, 4, 2));

    auto expr13 = _x1.not_in({"hello", "world"});
    CPPA_CHECK(ge_invoke(expr13, string("foo")));
    CPPA_CHECK(!ge_invoke(expr13, string("hello")));

    auto expr14 = _x1 + _x2;
    static_assert(std::is_same<decltype(ge_invoke(expr14, 1, 2)), int>::value,
                  "wrong return type");
    CPPA_CHECK_EQUAL(5, (ge_invoke(expr14, 2, 3)));

    auto expr15 = _x1 + _x2 + _x3;
    static_assert(std::is_same<decltype(ge_invoke(expr15,1,2,3)), int>::value,
                  "wrong return type");
    CPPA_CHECK_EQUAL(42, (ge_invoke(expr15, 7, 10, 25)));

    bool invoked = false;
    match("abc")
    (
        on<string>().when(_x1 == "abc") >> [&]()
        {
            invoked = true;
        }
    );
    if (!invoked) { CPPA_ERROR("match(\"abc\") failed"); }
    invoked = false;

    match(std::vector<int>{1, 2, 3})
    (
        on<int, int, int>().when(   _x1 + _x2 + _x3 == 6
                                 && _x2(is_even)
                                 && _x3 % 2 == 1) >> [&]()
        {
            invoked = true;
        }
    );
    if (!invoked) { CPPA_ERROR("match({1, 2, 3}) failed"); }
    invoked = false;

    string sum;
    match_each({"-h", "--version", "-wtf"})
    (
        on<string>().when(_x1.in({"-h", "--help"})) >> [&](string s)
        {
            sum += s;
        },
        on<string>().when(_x1 == "-v" || _x1 == "--version") >> [&](string s)
        {
            sum += s;
        },
        on<string>().when(_x1.starts_with("-")) >> [&](string const& str)
        {
            match_each(str.begin() + 1, str.end())
            (
                on<char>().when(_x1.in({'w', 't', 'f'})) >> [&](char c)
                {
                    sum += c;
                },
                others() >> [&]()
                {
                    CPPA_ERROR("unexpected match");
                }
            );
        },
        others() >> [&]()
        {
            CPPA_ERROR("unexpected match");
        }
    );
    CPPA_CHECK_EQUAL("-h--versionwtf", sum);

    match(5)
    (
        on<int>().when(_x1 < 6) >> [&](int i)
        {
            CPPA_CHECK_EQUAL(5, i);
            invoked = true;
        }
    );
    CPPA_CHECK(invoked);
    invoked = false;

    vector<string> vec{"a", "b", "c"};
    match(vec)
    (
        on("a", "b", val<string>) >> [&](string& str)
        {
            invoked = true;
            str = "C";
        }
    );
    if (!invoked) { CPPA_ERROR("match({\"a\", \"b\", \"c\"}) failed"); }
    CPPA_CHECK_EQUAL("C", vec.back());
    invoked = false;

    match_each(vec)
    (
        on("a") >> [&](string& str)
        {
            invoked = true;
            str = "A";
        }
    );
    if (!invoked) { CPPA_ERROR("match_each({\"a\", \"b\", \"C\"}) failed"); }
    CPPA_CHECK_EQUAL("A", vec.front());
    invoked = false;

    /*
    match(vec)
    (
        others() >> [&](any_tuple& tup)
        {
            if (detail::matches<string, string, string>(tup))
            {
                tup.get_as_mutable<string>(1) = "B";
            }
            else
            {
                CPPA_ERROR("matches<string, string, string>(tup) == false");
            }
            invoked = true;
        }
    );
    if (!invoked) { CPPA_ERROR("match({\"a\", \"b\", \"c\"}) failed"); }
    CPPA_CHECK_EQUAL(vec[1], "B");
    */

    vector<string> vec2{"a=0", "b=1", "c=2"};

    auto c2 = split(vec2.back(), '=');
    match(c2)
    (
        on("c", "2") >> [&]() { invoked = true; }
    );
    CPPA_CHECK_EQUAL(true, invoked);
    invoked = false;

    /*,
    int pmatches = 0;
    using std::placeholders::_1;
    pmatch_each(vec2.begin(), vec2.end(), std::bind(split, _1, '='))
    (
        on("a", val<string>) >> [&](string const& value)
        {
            CPPA_CHECK_EQUAL("0", value);
            CPPA_CHECK_EQUAL(0, pmatches);
            ++pmatches;
        },
        on("b", val<string>) >> [&](string const& value)
        {
            CPPA_CHECK_EQUAL("1", value);
            CPPA_CHECK_EQUAL(1, pmatches);
            ++pmatches;
        },
        on("c", val<string>) >> [&](string const& value)
        {
            CPPA_CHECK_EQUAL("2", value);
            CPPA_CHECK_EQUAL(2, pmatches);
            ++pmatches;
        }
        others() >> [](any_tuple const& value)
        {
            cout << to_string(value) << endl;
        }
    );
    CPPA_CHECK_EQUAL(3, pmatches);
    */

    return CPPA_TEST_RESULT;
}
