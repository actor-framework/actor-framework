#include <functional>

#include "test.hpp"
#include "cppa/cppa.hpp"
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

bool ascending(int a, int b, int c) {
    return a < b && b < c;
}

vector<const uniform_type_info*> to_vec(util::type_list<>, vector<const uniform_type_info*> vec = vector<const uniform_type_info*>{}) {
    return vec;
}

template<typename Head, typename... Tail>
vector<const uniform_type_info*> to_vec(util::type_list<Head, Tail...>, vector<const uniform_type_info*> vec = vector<const uniform_type_info*>{}) {
    vec.push_back(uniform_typeid<Head>());
    return to_vec(util::type_list<Tail...>{}, std::move(vec));
}

template<typename Fun>
class __ {

    typedef typename util::get_callable_trait<Fun>::type trait;

 public:

    __(Fun f, string annotation = "") : m_fun(f), m_annotation(annotation) { }

    __ operator[](const string& str) const {
        return {m_fun, str};
    }

    template<typename... Args>
    void operator()(Args&&... args) const {
        m_fun(std::forward<Args>(args)...);
    }

    void plot_signature() {
        auto vec = to_vec(typename trait::arg_types{});
        for (auto i : vec) {
            cout << i->name() << " ";
        }
        cout << endl;
    }

    const string& annotation() const {
        return m_annotation;
    }

 private:

    Fun m_fun;
    string m_annotation;

};

template<typename Fun>
__<Fun> get__(Fun f) {
    return {f};
}

struct fobaz : sb_actor<fobaz> {

    behavior init_state;

    void vfun() {
        cout << "fobaz::mfun" << endl;
    }

    void ifun(int i) {
        cout << "fobaz::ifun(" << i << ")" << endl;
    }

    fobaz() {
        init_state = (
            on<int>() >> [=](int i) { ifun(i); },
            others() >> std::function<void()>{std::bind(&fobaz::vfun, this)}
        );
    }

};

size_t test__match() {
    CPPA_TEST(test__match);

    using namespace std::placeholders;
    using namespace cppa::placeholders;

    /*
    auto x_ = get__([](int, float) { cout << "yeeeehaaaa!" << endl; })["prints 'yeeeehaaaa'"];
    cout << "x_: "; x_(1, 1.f);
    cout << "x_.annotation() = " << x_.annotation() << endl;
    cout << "x_.plot_signature: "; x_.plot_signature();

    auto fun = (
        on<int>() >> [](int i) {
            cout << "i = " << i << endl;
        },
        after(std::chrono::seconds(0)) >> []() {
            cout << "no int found in mailbox" << endl;
        }
    );
    */

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

    std::string expr16_str;// = "expr16";
    auto expr16_a = _x1.size();
    auto expr16_b = _x1.front() == 'e';
    CPPA_CHECK_EQUAL(false, ge_invoke(expr16_b, expr16_str));
    CPPA_CHECK_EQUAL(0, ge_invoke(expr16_a, expr16_str));
    expr16_str = "expr16";
    CPPA_CHECK_EQUAL(true, ge_invoke(expr16_b, expr16_str));
    CPPA_CHECK_EQUAL(expr16_str.size(), ge_invoke(expr16_a, expr16_str));
    expr16_str.front() = '_';
    CPPA_CHECK_EQUAL(false, ge_invoke(expr16_b, expr16_str));

    int expr17_value = 42;
    auto expr17 = gref(expr17_value) == 42;
    CPPA_CHECK_EQUAL(true, ge_invoke(expr17));
    expr17_value = 0;
    CPPA_CHECK_EQUAL(false, ge_invoke(expr17));

    int expr18_value = 42;
    auto expr18_a = gref(expr18_value) == 42;
    CPPA_CHECK_EQUAL(true, ge_invoke(expr18_a));
    expr18_value = 0;
    CPPA_CHECK_EQUAL(false, ge_invoke(expr18_a));
    auto expr18_b = gref(expr18_value) == _x1;
    auto expr18_c = std::ref(expr18_value) == _x1;
    CPPA_CHECK_EQUAL(true, ge_invoke(expr18_b, 0));
    CPPA_CHECK_EQUAL(true, ge_invoke(expr18_c, 0));


    bool invoked = false;
    auto kvp_split1 = [](const string& str) -> vector<string> {
        auto pos = str.find('=');
        if (pos != string::npos && pos == str.rfind('=')) {
            return vector<string>{str.substr(0, pos), str.substr(pos+1)};
        }
        return {};
    };
    /*
    auto kvp_split2 = [](const string& str) -> option<vector<string> > {
        auto pos = str.find('=');
        if (pos != string::npos && pos == str.rfind('=')) {
            return vector<string>{str.substr(0, pos-1), str.substr(pos+1)};
        }
        return {};
    };
    */

    match("value=42") (
        on(kvp_split1).when(_x1.not_empty()) >> [&](const vector<string>& vec) {
            CPPA_CHECK_EQUAL(vec[0], "value");
            CPPA_CHECK_EQUAL(vec[1], "42");
            invoked = true;
        }
    );
    CPPA_CHECK(invoked);
    invoked = false;

    auto toint = [](const string& str) -> option<int> {
        char* endptr = nullptr;
        int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
        if (endptr != nullptr && *endptr == '\0') {
            return result;
        }
        return {};
    };
    match("42") (
        on(toint) >> [&](int i) {
            CPPA_CHECK_EQUAL(i, 42);
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
    if (!invoked) { CPPA_ERROR("match(\"abc\") failed"); }
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
    if (!invoked) { CPPA_ERROR("match({1, 2, 3}) failed"); }
    invoked = false;

    string sum;
    match_each({"-h", "--version", "-wtf"}) (
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
                others() >> [&]() {
                    CPPA_ERROR("unexpected match");
                }
            );
        },
        others() >> [&]() {
            CPPA_ERROR("unexpected match");
        }
    );
    CPPA_CHECK_EQUAL("-h--versionwtf", sum);

    match(5) (
        on<int>().when(_x1 < 6) >> [&](int i) {
            CPPA_CHECK_EQUAL(5, i);
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
    if (!invoked) { CPPA_ERROR("match({\"a\", \"b\", \"c\"}) failed"); }
    CPPA_CHECK_EQUAL("C", vec.back());
    invoked = false;

    match_each(vec) (
        on("a") >> [&](string& str) {
            invoked = true;
            str = "A";
        }
    );
    if (!invoked) { CPPA_ERROR("match_each({\"a\", \"b\", \"C\"}) failed"); }
    CPPA_CHECK_EQUAL("A", vec.front());
    invoked = false;

    /*
    match(vec) (
        others() >> [&](any_tuple& tup) {
            if (detail::matches<string, string, string>(tup)) {
                tup.get_as_mutable<string>(1) = "B";
            }
            else {
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
    match(c2) (
        on("c", "2") >> [&]() { invoked = true; }
    );
    CPPA_CHECK_EQUAL(true, invoked);
    invoked = false;

    /*,
    int pmatches = 0;
    using std::placeholders::_1;
    match_each(vec2.begin(), vec2.end(), std::bind(split, _1, '=')) (
        on("a", val<string>) >> [&](const string& value) {
            CPPA_CHECK_EQUAL("0", value);
            CPPA_CHECK_EQUAL(0, pmatches);
            ++pmatches;
        },
        on("b", val<string>) >> [&](const string& value) {
            CPPA_CHECK_EQUAL("1", value);
            CPPA_CHECK_EQUAL(1, pmatches);
            ++pmatches;
        },
        on("c", val<string>) >> [&](const string& value) {
            CPPA_CHECK_EQUAL("2", value);
            CPPA_CHECK_EQUAL(2, pmatches);
            ++pmatches;
        }
        others() >> [](const any_tuple& value) {
            cout << to_string(value) << endl;
        }
    );
    CPPA_CHECK_EQUAL(3, pmatches);
    */

    return CPPA_TEST_RESULT;
}
