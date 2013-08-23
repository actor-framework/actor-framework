#include <iostream>
#include <functional>

#include "test.hpp"
#include "cppa/cppa.hpp"
#include "cppa/on.hpp"
#include "cppa/match.hpp"
#include "cppa/announce.hpp"
#include "cppa/to_string.hpp"
#include "cppa/guard_expr.hpp"

using namespace std;
using namespace cppa;

bool is_even(int i) { return i % 2 == 0; }

/*
 *
 * projection:
 *
 * on(_x.rsplit("--(.*)=(.*)")) >> [](std::vector<std::string> matched_groups)
 *
 */

struct dummy {

    inline operator actor_ptr() const { return nullptr; }
    inline actor_ptr as_facade() const { return nullptr; }

};

inline dummy operator|(const dummy&, const dummy&) { return {}; }

template<typename T>
dummy operator|(const dummy& lhs, const T&) {
    return lhs;
}

template<typename T>
dummy operator|(const T&, const dummy& rhs) {
    return rhs;
}

dummy slice(int,int) { return {}; }
dummy prepend(atom_value) { return {}; }
dummy consume(atom_value) { return {}; }
dummy split(int) { return {}; }
dummy join(int) { return {}; }

void example() {
    // {}: tuple
    // []: list
    // 'a': atom 'a' == atom("a")
    // x => y: replies to x with y

    {
        // for starters: get the internally used user-ID from a key-value store
        // and get the user role, e.g., "admin", from the user database

        // {'get', key} => {user_id, [tags]}
        actor_ptr kvstore;
        // {'info', user_id} => {name, address, phone, role}
        actor_ptr userinfo;
        // {username} => {role}
        auto query = kvstore | slice(0,0) | prepend(atom("info")) | userinfo | slice(3,3);
        // check whether Dirty Harry has admin access
        sync_send(query, atom("get"), "Dirty Harry").then(
            on("admin") >> [] {
                // grant access
            },
            others() >> [] {
                // deny access (well, no one denies Dirty Harry access anyways)
            }
        );
        // creates a new actor providing the interface {username} => {role}
        actor_ptr roles = query.as_facade();
    }

    {
        // we have an actor representing a database for orders and we want
        // to have the net price of all orders from Dirty Harry

        // {'orders', user_id} => {[order_id]}
        // {'price', order_id} => {net_price}
        actor_ptr orders;
        // {'prices', user_id} => {[net_price]}
        auto query = consume(atom("prices"))
                   | prepend(atom("orders"))
                   | orders
                   | prepend(atom("price"))
                   | split(1)
                   | orders
                   | join(0);
        sync_send(query, atom("prices"), "Dirty Harry").then(
            [](const vector<float>& prices) {
                cout << "Dirty Harry has ordered products "
                     << "with a total net price of $"
                     << accumulate(prices.begin(), prices.end(), 0)
                     << endl;
            }
        );
    }
}

template<typename T>
struct type_token { typedef T type; };

template<typename... Ts>
struct variant;

template<typename T>
struct variant<T> {

    T& get(type_token<T>) { return m_value; }

    T m_value;

};

template<typename T0, typename T1>
struct variant<T0, T1> {

 public:

    T0& get(type_token<T0>) { return m_v0; }
    T1& get(type_token<T1>) { return m_v1; }

    variant() : m_type{0}, m_v0{} { }

    ~variant() {
        switch (m_type) {
            case 0: m_v0.~T0(); break;
            case 1: m_v1.~T1(); break;
        }
    }

 private:

    size_t m_type;

    union { T0 m_v0; T1 m_v1; };

};

template<typename T, typename... Ts>
T& get(variant<Ts...>& v) {
    return v.get(type_token<T>{});
}

int main() {
    CPPA_TEST(test_match);

    using namespace std::placeholders;
    using namespace cppa::placeholders;

    variant<int, double> rofl;
    get<int>(rofl) = 42;
    cout << "rofl = " << get<int>(rofl) << endl;

    auto ascending = [](int a, int b, int c) -> bool {
        return a < b && b < c;
    };

    auto expr0_a = gcall(ascending, _x1, _x2, _x3);
    CPPA_CHECK(ge_invoke(expr0_a, 1, 2, 3));
    CPPA_CHECK(!ge_invoke(expr0_a, 3, 2, 1));

    int ival0 = 2;
    auto expr0_b = gcall(ascending, _x1, std::ref(ival0), _x2);
    CPPA_CHECK(ge_invoke(expr0_b, 1, 3));
    ++ival0;
    CPPA_CHECK(!ge_invoke(expr0_b, 1, 3));

    auto expr0_c = gcall(ascending, 10, _x1, 30);
    CPPA_CHECK(!ge_invoke(expr0_c, 10));
    CPPA_CHECK(ge_invoke(expr0_c, 11));

    auto expr1 = _x1 + _x2;
    auto expr2 = _x1 + _x2 < _x3;
    auto expr3 = _x1 % _x2 == 0;
    CPPA_CHECK_EQUAL((ge_invoke(expr1, 2, 3)), 5);
    //CPPA_CHECK(ge_invoke(expr2, 1, 2, 4));
    CPPA_CHECK(expr2(1, 2, 4));
    CPPA_CHECK_EQUAL((ge_invoke(expr1, std::string{"1"}, std::string{"2"})), "12");
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
    CPPA_CHECK_EQUAL((ge_invoke(expr14, 2, 3)), 5);

    auto expr15 = _x1 + _x2 + _x3;
    static_assert(std::is_same<decltype(ge_invoke(expr15, 1, 2, 3)), int>::value,
                  "wrong return type");
    CPPA_CHECK_EQUAL((ge_invoke(expr15, 7, 10, 25)), 42);

    std::string expr16_str;// = "expr16";
    auto expr16_a = _x1.size();
    auto expr16_b = _x1.front() == 'e';
    CPPA_CHECK_EQUAL((ge_invoke(expr16_b, expr16_str)), false);
    CPPA_CHECK_EQUAL((ge_invoke(expr16_a, expr16_str)), 0);
    expr16_str = "expr16";
    CPPA_CHECK_EQUAL((ge_invoke(expr16_b, expr16_str)), true);
    CPPA_CHECK_EQUAL((ge_invoke(expr16_a, expr16_str)), expr16_str.size());
    expr16_str.front() = '_';
    CPPA_CHECK_EQUAL((ge_invoke(expr16_b, expr16_str)), false);

    int expr17_value = 42;
    auto expr17 = gref(expr17_value) == 42;
    CPPA_CHECK_EQUAL(ge_invoke(expr17), true);
    expr17_value = 0;
    CPPA_CHECK_EQUAL(ge_invoke(expr17), false);

    int expr18_value = 42;
    auto expr18_a = gref(expr18_value) == 42;
    CPPA_CHECK_EQUAL(ge_invoke(expr18_a), true);
    expr18_value = 0;
    CPPA_CHECK_EQUAL(ge_invoke(expr18_a), false);
    auto expr18_b = gref(expr18_value) == _x1;
    auto expr18_c = std::ref(expr18_value) == _x1;
    CPPA_CHECK_EQUAL((ge_invoke(expr18_b, 0)), true);
    CPPA_CHECK_EQUAL((ge_invoke(expr18_c, 0)), true);


    bool invoked = false;
    auto kvp_split1 = [](const string& str) -> vector<string> {
        auto pos = str.find('=');
        if (pos != string::npos && pos == str.rfind('=')) {
            return vector<string>{str.substr(0, pos), str.substr(pos+1)};
        }
        return {};
    };

    match("value=42") (
        on(kvp_split1).when(_x1.not_empty()) >> [&](const vector<string>& vec) {
            CPPA_CHECK_EQUAL("value", vec[0]);
            CPPA_CHECK_EQUAL("42", vec[1]);
            invoked = true;
        }
    );
    CPPA_CHECK(invoked);
    invoked = false;

    auto toint = [](const string& str) -> optional<int> {
        char* endptr = nullptr;
        int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
        if (endptr != nullptr && *endptr == '\0') {
            return result;
        }
        return none;
    };
    match("42") (
        on(toint) >> [&](int i) {
            CPPA_CHECK_EQUAL(42, i);
            invoked = true;
        }
    );
    CPPA_CHECK(invoked);
    invoked = false;

    match("abc") (
        on<string>().when(_x1 == "abc") >> [&]() {
            invoked = true;
        }
    );
    if (!invoked) { CPPA_FAILURE("match(\"abc\") failed"); }
    invoked = false;

    bool disable_case1 = true;
    bool case1_invoked = false;
    bool case2_invoked = false;
    auto expr19 = (
        on<anything>().when(gref(disable_case1) == false) >> [&]() {
            case1_invoked = true;
        },
        on<anything>() >> [&]() {
            case2_invoked = true;
        }
    );
    any_tuple expr19_tup = make_cow_tuple("hello guard!");
    expr19(expr19_tup);
    CPPA_CHECK(!case1_invoked);
    CPPA_CHECK(case2_invoked);

    partial_function expr20 = expr19;
    case1_invoked = false;
    case2_invoked = false;
    disable_case1 = false;
    expr20(expr19_tup);
    CPPA_CHECK(case1_invoked);
    CPPA_CHECK(!case2_invoked);

    std::vector<int> expr21_vec_a{1, 2, 3};
    std::vector<int> expr21_vec_b{1, 0, 2};
    auto vec_sorted = [](const std::vector<int>& vec) {
        return std::is_sorted(vec.begin(), vec.end());
    };
    auto expr21 = gcall(vec_sorted, _x1);
    CPPA_CHECK(ge_invoke(expr21, expr21_vec_a));
    CPPA_CHECK(!ge_invoke(expr21, expr21_vec_b));

    auto expr22 = _x1.empty() && _x2.not_empty();
    CPPA_CHECK(ge_invoke(expr22, std::string(""), std::string("abc")));

    match(std::vector<int>{1, 2, 3}) (
        on<int, int, int>().when(   _x1 + _x2 + _x3 == 6
                                 && _x2(is_even)
                                 && _x3 % 2 == 1) >> [&]() {
            invoked = true;
        }
    );
    if (!invoked) { CPPA_FAILURE("match({1, 2, 3}) failed"); }
    invoked = false;

    string sum;
    vector<string> sum_args = { "-h", "--version", "-wtf" };
    match_each(begin(sum_args), end(sum_args)) (
        on<string>().when(_x1.in({"-h", "--help"})) >> [&](string s) {
            sum += s;
        },
        on<string>().when(_x1 == "-v" || _x1 == "--version") >> [&](string s) {
            sum += s;
        },
        on<string>().when(_x1.starts_with("-")) >> [&](const string& str) {
            match_each(str.begin() + 1, str.end()) (
                on<char>().when(_x1.in({'w', 't', 'f'})) >> [&](char c) {
                    sum += c;
                },
                on<char>() >> CPPA_FAILURE_CB("guard didn't match"),
                others() >> CPPA_FAILURE_CB("unexpected match")
            );
        },
        others() >> CPPA_FAILURE_CB("unexpected match")
    );
    CPPA_CHECK_EQUAL(sum, "-h--versionwtf");

    match(5) (
        on<int>().when(_x1 < 6) >> [&](int i) {
            CPPA_CHECK_EQUAL(i, 5);
            invoked = true;
        }
    );
    CPPA_CHECK(invoked);
    invoked = false;

    vector<string> vec{"a", "b", "c"};
    match(vec) (
        on("a", "b", val<string>) >> [&](string& str) {
            invoked = true;
            str = "C";
        }
    );
    if (!invoked) { CPPA_FAILURE("match({\"a\", \"b\", \"c\"}) failed"); }
    CPPA_CHECK_EQUAL(vec.back(), "C");
    invoked = false;

    match_each(begin(vec), end(vec)) (
        on("a") >> [&](string& str) {
            invoked = true;
            str = "A";
        }
    );
    if (!invoked) { CPPA_FAILURE("match_each({\"a\", \"b\", \"C\"}) failed"); }
    CPPA_CHECK_EQUAL(vec.front(), "A");
    invoked = false;

    vector<string> vec2{"a=0", "b=1", "c=2"};

    auto c2 = split(vec2.back(), '=');
    match(c2) (
        on("c", "2") >> [&]() { invoked = true; }
    );
    CPPA_CHECK_EQUAL(invoked, true);
    invoked = false;

    // let's get the awesomeness started
    bool success = false;

    istringstream iss("hello world");
    success = match_stream<string>(iss) (
        on("hello", "world") >> CPPA_CHECKPOINT_CB()
    );
    CPPA_CHECK_EQUAL(success, true);

    auto extract_name = [](const string& kvp) -> optional<string> {
        auto vec = split(kvp, '=');
        if (vec.size() == 2) {
            if (vec.front() == "--name") {
                return vec.back();
            }
        }
        return none;
    };

    const char* svec[] = {"-n", "foo", "--name=bar", "-p", "2"};

    success = match_stream<string>(begin(svec), end(svec)) (
        (on("-n", arg_match) || on(extract_name)) >> [](const string& name) -> bool {
            CPPA_CHECK(name == "foo" || name == "bar");
            return name == "foo" || name == "bar";
        },
        on("-p", arg_match) >> [&](const string& port) -> match_hint {
            auto i = toint(port);
            if (i) {
                CPPA_CHECK_EQUAL(*i, 2);
                return match_hint::handle;
            }
            else return match_hint::skip;
        },
        on_arg_match >> [](const string& arg) -> match_hint {
            CPPA_FAILURE("unexpected string: " << arg);
            return match_hint::skip;
        }
    );
    CPPA_CHECK_EQUAL(success, true);

    CPPA_PRINT("check combined partial function matching");

    std::string last_invoked_fun;

#   define check(pf, tup, str) {                                               \
        last_invoked_fun = "";                                                 \
        pf(tup);                                                               \
        CPPA_CHECK_EQUAL(last_invoked_fun, str);                               \
    }

    partial_function pf0 = (
        on<int, int>() >> [&] { last_invoked_fun = "<int, int>@1"; },
        on<float>() >> [&] { last_invoked_fun = "<float>@2"; }
    );

    auto pf1 = pf0.or_else(
        on<int, int>() >> [&] { last_invoked_fun = "<int, int>@3"; },
        on<string>() >> [&] { last_invoked_fun = "<string>@4"; }
    );

    check(pf0, make_any_tuple(1, 2), "<int, int>@1");
    check(pf1, make_any_tuple(1, 2), "<int, int>@1");
    check(pf0, make_any_tuple("hi"), "");
    check(pf1, make_any_tuple("hi"), "<string>@4");

    CPPA_PRINT("check match expressions with boolean return value");

#   define bhvr_check(pf, tup, expected_result, str) {                         \
        last_invoked_fun = "";                                                 \
        CPPA_CHECK_EQUAL(pf(tup), expected_result);                            \
        CPPA_CHECK_EQUAL(last_invoked_fun, str);                               \
    }

    behavior bhvr1 = (
        on<int>() >> [&](int i) -> match_hint {
            last_invoked_fun = "<int>@1";
            return (i == 42) ? match_hint::skip : match_hint::handle;
        },
        on<float>() >> [&] {
            last_invoked_fun = "<float>@2";
        },
        on<int>() >> [&] {
            last_invoked_fun = "<int>@3";
        },
        others() >> [&] {
            last_invoked_fun = "<*>@4";
        }
    );

    bhvr_check(bhvr1, make_any_tuple(42), false, "<int>@1");
    bhvr_check(bhvr1, make_any_tuple(24), true, "<int>@1");
    bhvr_check(bhvr1, make_any_tuple(2.f), true, "<float>@2");
    bhvr_check(bhvr1, make_any_tuple(""), true, "<*>@4");

    partial_function pf11 {
        on_arg_match >> [&](int) { last_invoked_fun = "<int>@1"; }
    };

    partial_function pf12 {
        on_arg_match >> [&](int) { last_invoked_fun = "<int>@2"; },
        on_arg_match >> [&](float) { last_invoked_fun = "<float>@2"; }
    };

    auto pf13 = pf11.or_else(pf12);

    bhvr_check(pf13, make_any_tuple(42), true, "<int>@1");
    bhvr_check(pf13, make_any_tuple(42.24f), true, "<float>@2");

    return CPPA_TEST_RESULT();
}
