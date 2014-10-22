#include <iostream>
#include <functional>

#include "test.hpp"
#include "caf/all.hpp"

using namespace caf;
using namespace std;

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

template <class Expr, class... Ts>
bool invoked(int idx, Expr& expr, Ts&&... args) {
  expr(make_message(forward<Ts>(args)...));
  auto res = s_invoked[idx];
  s_invoked[idx] = false;
  auto e = end(s_invoked);
  res = res && find(begin(s_invoked), e, true) == e;
  reset();
  return res;
}

template <class Expr, class... Ts>
bool not_invoked(Expr& expr, Ts&&... args) {
  expr(make_message(forward<Ts>(args)...));
  auto e = end(s_invoked);
  auto res = find(begin(s_invoked), e, true) == e;
  fill(begin(s_invoked), end(s_invoked), false);
  return res;
}

function<void()> f(int idx) {
  return [=] {
    s_invoked[idx] = true;
  };
}

void test_atoms() {
  {
    auto expr = on(atom("hi")) >> f(0);
    CAF_CHECK(invoked(0, expr, atom("hi")));
    CAF_CHECK(not_invoked(expr, atom("ho")));
    CAF_CHECK(not_invoked(expr, atom("hi"), atom("hi")));
    CAF_CHECK(not_invoked(expr, "hi"));
  }
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
    CAF_CHECK(invoked(0, expr, 42));
    CAF_CHECK(guard_called);
  }
  // check forwarding optional<const string&> from guard
  {
    auto expr = (
      on(starts_with("--")) >> [](const string& str) {
        CAF_CHECK_EQUAL(str, "help");
        s_invoked[0] = true;
      }
    );
    CAF_CHECK(invoked(0, expr, "--help"));
    CAF_CHECK(not_invoked(expr, "-help"));
    CAF_CHECK(not_invoked(expr, "--help", "--help"));
    CAF_CHECK(not_invoked(expr, 42));
  }
  // check projection
  {
    auto expr = (
      on(toint) >> [](int i) {
        CAF_CHECK_EQUAL(i, 42);
        s_invoked[0] = true;
      }
    );
    CAF_CHECK(invoked(0, expr, "42"));
    CAF_CHECK(not_invoked(expr, "42f"));
    CAF_CHECK(not_invoked(expr, "42", "42"));
    CAF_CHECK(not_invoked(expr, 42));
  }
}

int main() {
  CAF_TEST(test_match);
  test_atoms();
  test_custom_projections();
  return CAF_TEST_RESULT();
}
