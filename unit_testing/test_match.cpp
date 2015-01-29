#include <iostream>
#include <functional>

#include "test.hpp"
#include "caf/all.hpp"

using namespace caf;
using namespace std;

using hi_atom = atom_constant<atom("hi")>;
using ho_atom = atom_constant<atom("ho")>;

optional<int> even_int(int i) {
  if (i % 2 == 0) {
    return i;
  }
  return none;
}

optional<const vector<int>&> sorted_vec(const vector<int>& vec) {
  if (is_sorted(vec.begin(), vec.end())) {
    return vec;
  }
  return none;
}

function<optional<string>(const string&)> starts_with(const string& s) {
  return [=](const string& str) -> optional<string> {
    cout << "starts_with guard called" << endl;
    if (str.size() > s.size() && str.compare(0, s.size(), s) == 0) {
      auto res = str.substr(s.size());
      return res;
    }
    return none;
  };
}

optional<vector<string>> kvp_split(const string& str) {
  auto pos = str.find('=');
  if (pos != string::npos && pos == str.rfind('=')) {
    return vector<string>{str.substr(0, pos), str.substr(pos+1)};
  }
  return none;
}

optional<int> toint(const string& str) {
  char* endptr = nullptr;
  int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
  if (endptr != nullptr && *endptr == '\0') {
    return result;
  }
  return none;
}

namespace {

bool s_invoked[] = {false, false, false, false};

} // namespace <anonymous>

void reset() {
  fill(begin(s_invoked), end(s_invoked), false);
}

void fill_mb(message_builder&) {
  // end of recursion
}

template <class T, class... Ts>
void fill_mb(message_builder& mb, const T& v, const Ts&... vs) {
  fill_mb(mb.append(v), vs...);
}

template <class Expr, class... Ts>
ptrdiff_t invoked(Expr& expr, const Ts&... args) {
  vector<message> msgs;
  msgs.push_back(make_message(args...));
  message_builder mb;
  fill_mb(mb, args...);
  msgs.push_back(mb.to_message());
  set<ptrdiff_t> results;
  for (auto& msg : msgs) {
    expr(msg);
    auto first = begin(s_invoked);
    auto last = end(s_invoked);
    auto i = find(begin(s_invoked), last, true);
    results.insert(i != last && count(i, last, true) == 1 ? distance(first, i)
                                                          : -1);
    reset();
  }
  if (results.size() > 1) {
    CAF_FAILURE("make_message() yielded a different result than "
                "message_builder(...).to_message()");
    return -2;
  }
  return *results.begin();
}

function<void()> f(int idx) {
  return [=] {
    s_invoked[idx] = true;
  };
}

void test_atoms() {
  auto expr = on(hi_atom::value) >> f(0);
  CAF_CHECK_EQUAL(invoked(expr, hi_atom::value), 0);
  CAF_CHECK_EQUAL(invoked(expr, ho_atom::value), -1);
  CAF_CHECK_EQUAL(invoked(expr, hi_atom::value, hi_atom::value), -1);
  message_handler expr2{
    [](hi_atom) {
      s_invoked[0] = true;
    },
    [](ho_atom) {
      s_invoked[1] = true;
    }
  };
  CAF_CHECK_EQUAL(invoked(expr2, ok_atom::value), -1);
  CAF_CHECK_EQUAL(invoked(expr2, hi_atom::value), 0);
  CAF_CHECK_EQUAL(invoked(expr2, ho_atom::value), 1);
}

void test_custom_projections() {
  // check whether projection is called
  {
    bool guard_called = false;
    auto guard = [&](int arg) -> optional<int> {
      guard_called = true;
      return arg;
    };
    auto expr = (
      on(guard) >> f(0)
    );
    CAF_CHECK_EQUAL(invoked(expr, 42), 0);
    CAF_CHECK_EQUAL(guard_called, true);
  }
  // check forwarding optional<const string&> from guard
  {
    auto expr = (
      on(starts_with("--")) >> [](const string& str) {
        CAF_CHECK_EQUAL(str, "help");
        s_invoked[0] = true;
      }
    );
    CAF_CHECK_EQUAL(invoked(expr, "--help"), 0);
    CAF_CHECK_EQUAL(invoked(expr, "-help"), -1);
    CAF_CHECK_EQUAL(invoked(expr, "--help", "--help"), -1);
    CAF_CHECK_EQUAL(invoked(expr, 42), -1);
  }
  // check projection
  {
    auto expr = (
      on(toint) >> [](int i) {
        CAF_CHECK_EQUAL(i, 42);
        s_invoked[0] = true;
      }
    );
    CAF_CHECK_EQUAL(invoked(expr, "42"), 0);
    CAF_CHECK_EQUAL(invoked(expr, "42f"), -1);
    CAF_CHECK_EQUAL(invoked(expr, "42", "42"), -1);
    CAF_CHECK_EQUAL(invoked(expr, 42), -1);
  }
}

struct wrapped_int {
  int value;
};

inline bool operator==(const wrapped_int& lhs, const wrapped_int& rhs) {
  return lhs.value == rhs.value;
}

void test_arg_match() {
  announce<wrapped_int>("wrapped_int", &wrapped_int::value);
  auto expr = on(42, arg_match) >> [](int i) {
    s_invoked[0] = true;
    CAF_CHECK_EQUAL(i, 1);
  };
  CAF_CHECK_EQUAL(invoked(expr, 42, 1.f), -1);
  CAF_CHECK_EQUAL(invoked(expr, 42), -1);
  CAF_CHECK_EQUAL(invoked(expr, 1, 42), -1);
  CAF_CHECK_EQUAL(invoked(expr, 42, 1), 0);
  auto expr2 = on("-a", arg_match) >> [](const string& value) {
    s_invoked[0] = true;
    CAF_CHECK_EQUAL(value, "b");
  };
  CAF_CHECK_EQUAL(invoked(expr2, "b", "-a"), -1);
  CAF_CHECK_EQUAL(invoked(expr2, "-a"), -1);
  CAF_CHECK_EQUAL(invoked(expr2, "-a", "b"), 0);
  auto expr3 = on(wrapped_int{42}, arg_match) >> [](wrapped_int i) {
    s_invoked[0] = true;
    CAF_CHECK_EQUAL(i.value, 1);
  };
  CAF_CHECK_EQUAL(invoked(expr3, wrapped_int{42}, 1.f), -1);
  CAF_CHECK_EQUAL(invoked(expr3, 42), -1);
  CAF_CHECK_EQUAL(invoked(expr3, wrapped_int{1}, wrapped_int{42}), -1);
  CAF_CHECK_EQUAL(invoked(expr3, 42, 1), -1);
  CAF_CHECK_EQUAL(invoked(expr3, wrapped_int{42}, wrapped_int{1}), 0);
}

int main() {
  CAF_TEST(test_match);
  test_atoms();
  test_custom_projections();
  test_arg_match();
  return CAF_TEST_RESULT();
}
