#include <iostream>
#include <functional>

#include "test.hpp"
#include "cppa/cppa.hpp"
#include "cppa/on.hpp"
#include "cppa/match.hpp"
#include "cppa/announce.hpp"
#include "cppa/to_string.hpp"

using namespace std;
using namespace cppa;
using namespace std::placeholders;
using namespace cppa::placeholders;

bool is_even(int i) { return i % 2 == 0; }

bool vec_sorted(const std::vector<int>& vec) {
  return std::is_sorted(vec.begin(), vec.end());
}

vector<string> kvp_split(const string& str) {
  auto pos = str.find('=');
  if (pos != string::npos && pos == str.rfind('=')) {
    return vector<string>{str.substr(0, pos), str.substr(pos+1)};
  }
  return vector<string>{};
}

optional<int> toint(const string& str) {
  char* endptr = nullptr;
  int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
  if (endptr != nullptr && *endptr == '\0') return result;
  return none;
}

template<size_t Pos, size_t Max>
struct index_token { };

template<size_t Max>
void foobar(index_token<Max, Max>) { }

template<size_t Pos, size_t Max>
void foobar(index_token<Pos, Max>) {
  cout << "foobar<" << Pos << "," << Max << ">" << endl;
  foobar(index_token<Pos+1, Max>{});
}

void test_simple_guards() {
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
  // user-defined expressions using '+', '<', '%'
  auto expr1 = _x1 + _x2;
  auto expr2 = _x1 + _x2 < _x3;
  auto expr3 = _x1 % _x2 == 0;
  CPPA_CHECK_EQUAL((ge_invoke(expr1, 2, 3)), 5);
  CPPA_CHECK(expr2(1, 2, 4));
  CPPA_CHECK_EQUAL((ge_invoke(expr1, std::string{"1"}, std::string{"2"})), "12");
  CPPA_CHECK(expr3(100, 2));
  // user-defined expressions using '||'
  auto expr4 = _x1 == "-h" || _x1 == "--help";
  CPPA_CHECK(ge_invoke(expr4, string("-h")));
  CPPA_CHECK(ge_invoke(expr4, string("--help")));
  CPPA_CHECK(!ge_invoke(expr4, string("-g")));
}

void test_container_guards() {
  auto expr5 = _x1.starts_with("--");
  CPPA_CHECK(ge_invoke(expr5, string("--help")));
  CPPA_CHECK(!ge_invoke(expr5, string("-help")));
  // container search (x in y)
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
  // container search (x in y) for initializer lists
  auto expr11 = _x1.in({"one", "two"});
  CPPA_CHECK(ge_invoke(expr11, string("one")));
  CPPA_CHECK(ge_invoke(expr11, string("two")));
  CPPA_CHECK(!ge_invoke(expr11, string("three")));
  // container search (x not in y)
  auto expr12 = _x1.not_in({"hello", "world"});
  CPPA_CHECK(ge_invoke(expr12, string("foo")));
  CPPA_CHECK(!ge_invoke(expr12, string("hello")));
  // user-defined function to evaluate whether container is sorted
  std::vector<int> expr21_vec_a{1, 2, 3};
  std::vector<int> expr21_vec_b{1, 0, 2};
  auto expr21 = gcall(vec_sorted, _x1);
  CPPA_CHECK(ge_invoke(expr21, expr21_vec_a));
  CPPA_CHECK(!ge_invoke(expr21, expr21_vec_b));
  auto expr22 = _x1.empty() && _x2.not_empty();
  CPPA_CHECK(ge_invoke(expr22, std::string(""), std::string("abc")));
}

void test_user_defined_expressions() {
  int ival = 5;
  auto expr8 = _x1 == ival;
  auto expr9 = _x1 == std::ref(ival);
  CPPA_CHECK(ge_invoke(expr8, 5));
  CPPA_CHECK(ge_invoke(expr9, 5));
  ival = 10;
  CPPA_CHECK(!ge_invoke(expr9, 5));
  CPPA_CHECK(ge_invoke(expr9, 10));
  // user-defined expressions w/ wide range of operators
  auto expr13 = _x1 * _x2 < _x3 - _x4;
  CPPA_CHECK(ge_invoke(expr13, 1, 1, 4, 2));
  auto expr14 = _x1 + _x2;
  static_assert(std::is_same<decltype(ge_invoke(expr14, 1, 2)), int>::value,
          "wrong return type");
  CPPA_CHECK_EQUAL((ge_invoke(expr14, 2, 3)), 5);
  auto expr15 = _x1 + _x2 + _x3;
  static_assert(std::is_same<decltype(ge_invoke(expr15, 1, 2, 3)), int>::value,
          "wrong return type");
  CPPA_CHECK_EQUAL((ge_invoke(expr15, 7, 10, 25)), 42);
  // user-defined operations on containers such as 'size' and 'front'
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
}

void test_gref() {
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
  bool enable_case1 = true;
  auto expr19 = (
    on<anything>().when(gref(enable_case1) == false) >> [] { return 1; },
    on<anything>() >> [] { return 2; }
  );
  message expr19_tup = make_cow_tuple("hello guard!");
  auto res19 = expr19(expr19_tup);
  CPPA_CHECK(get<int>(&res19) && get<int>(res19) == 2);
  message_handler expr20 = expr19;
  enable_case1 = false;
  CPPA_CHECK(expr20(expr19_tup) == make_message(1));
  message_handler expr21 {
    on(atom("add"), arg_match) >> [](int a, int b) {
      return a + b;
    }
  };
  CPPA_CHECK(expr21(make_message(atom("add"), 1, 2)) == make_message(3));
  CPPA_CHECK(!expr21(make_message(atom("sub"), 1, 2)));
}

void test_match_function() {
  auto res0 = match(5) (
    on_arg_match.when(_x1 < 6) >> [&](int i) {
      CPPA_CHECK_EQUAL(i, 5);
    }
  );
  CPPA_CHECK(!get<none_t>(&res0));
  auto res1 = match("value=42") (
    on(kvp_split).when(_x1.not_empty()) >> [&](const vector<string>& vec) {
      CPPA_CHECK_EQUAL("value", vec[0]);
      CPPA_CHECK_EQUAL("42", vec[1]);
    }
  );
  CPPA_CHECK(!get<none_t>(&res1));
  auto res2 = match("42") (
    on(toint) >> [&](int i) {
      CPPA_CHECK_EQUAL(42, i);
    }
  );
  CPPA_CHECK(!get<none_t>(&res2));
  auto res3 = match("abc") (
    on<string>().when(_x1 == "abc") >> [&] { }
  );
  CPPA_CHECK(!get<none_t>(&res3));
  CPPA_CHECK(get<unit_t>(&res3));
  // match vectors
  auto res4 = match(std::vector<int>{1, 2, 3}) (
    on<int, int, int>().when(   _x1 + _x2 + _x3 == 6
                 && _x2(is_even)
                 && _x3 % 2 == 1    ) >> [&] { }
  );
  CPPA_CHECK(!get<none_t>(&res4));
  vector<string> vec{"a", "b", "c"};
  auto res5 = match(vec) (
    on("a", "b", val<string>) >> [](string&) -> string {
      return "C";
    }
  );
  CPPA_CHECK_EQUAL(get<string>(res5), "C");
}

void test_match_each() {
  string sum;
  vector<string> sum_args = { "-h", "--version", "-wtf" };
  auto iter1 = match_each(begin(sum_args), end(sum_args)) (
    on<string>().when(_x1.in({"-h", "--help"})) >> [&](const string& s) {
      sum += s;
    },
    on<string>().when(_x1 == "-v" || _x1 == "--version") >> [&](const string& s) {
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
    }
  );
  CPPA_CHECK(iter1 == end(sum_args));
  CPPA_CHECK_EQUAL(sum, "-h--versionwtf");
  auto iter2 = match_each(begin(sum_args), end(sum_args)) (
    on("-h") >> [] { }
  );
  CPPA_CHECK(iter2 == (begin(sum_args) + 1));
}

void test_match_stream() {
  bool success = false;
  istringstream iss("hello world");
  success = match_stream<string>(iss) (
    on("hello", "world") >> CPPA_CHECKPOINT_CB()
  );
  CPPA_CHECK_EQUAL(success, true);
  auto extract_name = [](const string& kvp) -> optional<string> {
    auto vec = split(kvp, '=');
    if (vec.size() == 2 && vec.front() == "--name") return vec.back();
    return none;
  };
  const char* svec[] = {"-n", "foo", "--name=bar", "-p", "2"};
  success = match_stream<string>(begin(svec), end(svec)) (
    (on("-n", arg_match) || on(extract_name)) >> [](const string& name) -> bool {
      CPPA_CHECK(name == "foo" || name == "bar");
      return name == "foo" || name == "bar";
    },
    on("-p", toint) >> [&](int port) {
      CPPA_CHECK_EQUAL(port, 2);
    },
    on_arg_match >> [](const string& arg) -> skip_message_t {
      CPPA_FAILURE("unexpected string: " << arg);
      return {};
    }
  );
  CPPA_CHECK_EQUAL(success, true);
}

void test_behavior() {
  std::string last_invoked_fun;
#   define bhvr_check(pf, tup, expected_result, str) {             \
    last_invoked_fun = "";                         \
    CPPA_CHECK_EQUAL(static_cast<bool>(pf(tup)), expected_result);     \
    CPPA_CHECK_EQUAL(last_invoked_fun, str);                 \
  }
  auto not_42 = [](int i) -> optional<int> {
    if (i != 42) return i;
    return none;
  };
  behavior bhvr1 {
    on(not_42) >> [&](int) {
      last_invoked_fun = "<int>@1";
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
  };
  bhvr_check(bhvr1, make_message(42), true, "<int>@3");
  bhvr_check(bhvr1, make_message(24), true, "<int>@1");
  bhvr_check(bhvr1, make_message(2.f), true, "<float>@2");
  bhvr_check(bhvr1, make_message(""), true, "<*>@4");
  message_handler pf0 {
    on<int, int>() >> [&] { last_invoked_fun = "<int, int>@1"; },
    on<float>() >> [&] { last_invoked_fun = "<float>@2"; }
  };
  auto pf1 = pf0.or_else (
    on<int, int>() >> [&] { last_invoked_fun = "<int, int>@3"; },
    on<string>() >> [&] { last_invoked_fun = "<string>@4"; }
  );
  bhvr_check(pf0, make_message(1, 2), true, "<int, int>@1");
  bhvr_check(pf1, make_message(1, 2), true, "<int, int>@1");
  bhvr_check(pf0, make_message("hi"), false, "");
  bhvr_check(pf1, make_message("hi"), true, "<string>@4");
  message_handler pf11 {
    on_arg_match >> [&](int) { last_invoked_fun = "<int>@1"; }
  };
  message_handler pf12 {
    on_arg_match >> [&](int) { last_invoked_fun = "<int>@2"; },
    on_arg_match >> [&](float) { last_invoked_fun = "<float>@2"; }
  };
  auto pf13 = pf11.or_else(pf12);
  bhvr_check(pf13, make_message(42), true, "<int>@1");
  bhvr_check(pf13, make_message(42.24f), true, "<float>@2");
}

void test_pattern_matching() {
   auto res = match(make_message(42, 4.2f)) (
    on(42, arg_match) >> [](double d) {
      return d;
    },
    on(42, arg_match) >> [](float f) {
      return f;
    }
  );
  CPPA_CHECK(get<float>(&res) && util::safe_equal(get<float>(res), 4.2f));
  auto res2 = match(make_message(23, 4.2f)) (
    on(42, arg_match) >> [](double d) {
      return d;
    },
    on(42, arg_match) >> [](float f) {
      return f;
    }
  );
  CPPA_CHECK(get<none_t>(&res2));
}

inline void make_dynamically_typed_impl(detail::object_array&) { }

template<typename T0, typename... Ts>
void make_dynamically_typed_impl(detail::object_array& arr, T0&& arg0, Ts&&... args) {
  using type = typename detail::strip_and_convert<T0>::type;
  arr.push_back(make_uniform_value<type>(uniform_typeid<type>(),
                       std::forward<T0>(arg0)));
  make_dynamically_typed_impl(arr, std::forward<Ts>(args)...);
}

template<typename... Ts>
message make_dynamically_typed(Ts&&... args) {
  auto oarr = new detail::object_array;
  make_dynamically_typed_impl(*oarr, std::forward<Ts>(args)...);
  return message{static_cast<message::raw_ptr>(oarr)};
}

void test_wildcards() {
  // leading wildcard
  auto expr0 = (
    on(any_vals, arg_match) >> [](int a, int b) {
      CPPA_CHECK_EQUAL(a, 1);
      CPPA_CHECK_EQUAL(b, 2);
    }
  );
  CPPA_CHECK_NOT(expr0(1));
  CPPA_CHECK(expr0(1, 2));
  CPPA_CHECK(expr0(0, 1, 2));
  message_handler pf0 = expr0;
  CPPA_CHECK(not pf0(make_message(1)));
  CPPA_CHECK(pf0(make_message(1, 2)));
  CPPA_CHECK(pf0(make_message(0, 1, 2)));
  CPPA_CHECK(not pf0(make_dynamically_typed(1)));
  CPPA_CHECK(pf0(make_dynamically_typed(1, 2)));
  CPPA_CHECK(pf0(make_dynamically_typed(0, 1, 2)));
  behavior bhvr0 = expr0;
  CPPA_CHECK(bhvr0(make_message(1, 2)));
  CPPA_CHECK(bhvr0(make_message(0, 1, 2)));
  CPPA_CHECK(bhvr0(make_dynamically_typed(1, 2)));
  CPPA_CHECK(bhvr0(make_dynamically_typed(0, 1, 2)));
  // wildcard in between
  message_handler pf1 {
    on<int, anything, int>() >> [](int a, int b) {
      CPPA_CHECK_EQUAL(a, 10);
      CPPA_CHECK_EQUAL(b, 20);
    }
  };
  CPPA_CHECK(pf1(make_message(10, 20)));
  CPPA_CHECK(pf1(make_message(10, 15, 20)));
  CPPA_CHECK(pf1(make_message(10, "hello world", 15, 20)));
  CPPA_CHECK(pf1(make_dynamically_typed(10, 20)));
  CPPA_CHECK(pf1(make_dynamically_typed(10, 15, 20)));
  CPPA_CHECK(pf1(make_dynamically_typed(10, "hello world", 15, 20)));
  // multiple wildcards: one leading
  message_handler pf2 {
    on<anything, int, anything, int>() >> [](int a, int b) {
      CPPA_CHECK_EQUAL(a, 10);
      CPPA_CHECK_EQUAL(b, 20);
    }
  };
  CPPA_CHECK(pf2(make_message(10, 20)));
  CPPA_CHECK(pf2(make_message(10, 15, 20)));
  CPPA_CHECK(pf2(make_message(10, "hello world", 15, 20)));
  CPPA_CHECK(pf2(make_message("hello world", 10, 20)));
  CPPA_CHECK(pf2(make_message("hello", 10, "world", 15, 20)));
  CPPA_CHECK(pf2(make_dynamically_typed(10, 20)));
  CPPA_CHECK(pf2(make_dynamically_typed(10, 15, 20)));
  CPPA_CHECK(pf2(make_dynamically_typed(10, "hello world", 15, 20)));
  CPPA_CHECK(pf2(make_dynamically_typed("hello world", 10, 20)));
  CPPA_CHECK(pf2(make_dynamically_typed("hello", 10, "world", 15, 20)));
  // multiple wildcards: in between
  message_handler pf3 {
    on<int, anything, int, anything, int>() >> [](int a, int b, int c) {
      CPPA_CHECK_EQUAL(a, 10);
      CPPA_CHECK_EQUAL(b, 20);
      CPPA_CHECK_EQUAL(c, 30);
    }
  };
  CPPA_CHECK(pf3(make_message(10, 20, 30)));
  CPPA_CHECK(pf3(make_message(10, 20, 25, 30)));
  CPPA_CHECK(pf3(make_message(10, "hello", 20, "world", 30)));
  CPPA_CHECK(pf3(make_message(10, "hello", 20, "world", 1, 2, 30)));
  CPPA_CHECK(pf3(make_dynamically_typed(10, 20, 30)));
  CPPA_CHECK(pf3(make_dynamically_typed(10, 20, 25, 30)));
  CPPA_CHECK(pf3(make_dynamically_typed(10, "hello", 20, "world", 30)));
  CPPA_CHECK(pf3(make_dynamically_typed(10, "hello", 20, "world", 1, 2, 30)));
  // multiple wildcards: one trailing
  message_handler pf4 {
    on<int, anything, int, anything>() >> [](int a, int b) {
      CPPA_CHECK_EQUAL(a, 10);
      CPPA_CHECK_EQUAL(b, 20);
    }
  };
  CPPA_CHECK(pf4(make_message(10, 20, 30)));
  CPPA_CHECK(pf4(make_message(10, 20, 25, 30)));
  CPPA_CHECK(pf4(make_message(10, "hello", 20, "world", 30)));
  CPPA_CHECK(pf4(make_message(10, "hello", 20, "world", 1, 2, 30)));
  CPPA_CHECK(pf4(make_dynamically_typed(10, 20, 30)));
  CPPA_CHECK(pf4(make_dynamically_typed(10, 20, 25, 30)));
  CPPA_CHECK(pf4(make_dynamically_typed(10, "hello", 20, "world", 30)));
  CPPA_CHECK(pf4(make_dynamically_typed(10, "hello", 20, "world", 1, 2, 30)));
  // multiple wildcards: leading and trailing
  message_handler pf5 {
    on<anything, int, anything>() >> [](int a) {
      CPPA_CHECK_EQUAL(a, 10);
    }
  };
  CPPA_CHECK(pf5(make_message(10, 20, 30)));
  CPPA_CHECK(pf5(make_message("hello", 10, "world", 30)));
  CPPA_CHECK(pf5(make_message("hello", "world", 10)));
  CPPA_CHECK(pf5(make_dynamically_typed(10, 20, 30)));
  CPPA_CHECK(pf5(make_dynamically_typed("hello", 10, "world", 30)));
  CPPA_CHECK(pf5(make_dynamically_typed("hello", "world", 10)));
}

int main() {
  CPPA_TEST(test_match);
  test_wildcards();
  test_simple_guards();
  test_container_guards();
  test_user_defined_expressions();
  test_gref();
  test_match_function();
  test_match_each();
  test_match_stream();
  test_behavior();
  test_pattern_matching();
  return CPPA_TEST_RESULT();
}
