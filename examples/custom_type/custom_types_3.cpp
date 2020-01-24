// Showcases custom message types that cannot provide
// friend access to the inspect() function.

// Manual refs: 20-49, 76-103 (TypeInspection)

#include <iostream>
#include <utility>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::make_pair;

using namespace caf;

namespace {

// identical to our second custom type example, but
// no friend access for `inspect`
class foo {
public:
  foo(int a0 = 0, int b0 = 0) : a_(a0), b_(b0) {
    // nop
  }

  foo(const foo&) = default;
  foo& operator=(const foo&) = default;

  int a() const {
    return a_;
  }

  void set_a(int val) {
    a_ = val;
  }

  int b() const {
    return b_;
  }

  void set_b(int val) {
    b_ = val;
  }

private:
  int a_;
  int b_;
};

// A lightweight scope guard implementation.
template <class Fun>
class scope_guard {
public:
  scope_guard(Fun f) : fun_(std::move(f)), enabled_(true) {
    // nop
  }

  scope_guard(scope_guard&& x) : fun_(std::move(x.fun_)), enabled_(x.enabled_) {
    x.enabled_ = false;
  }

  ~scope_guard() {
    if (enabled_)
      fun_();
  }

private:
  Fun fun_;
  bool enabled_;
};

// Creates a guard that executes `f` as soon as it goes out of scope.
template <class Fun>
scope_guard<Fun> make_scope_guard(Fun f) {
  return {std::move(f)};
}

template <class Inspector>
typename std::enable_if<Inspector::reads_state,
                        typename Inspector::result_type>::type
inspect(Inspector& f, foo& x) {
  return f(meta::type_name("foo"), x.a(), x.b());
}

template <class Inspector>
typename std::enable_if<Inspector::writes_state,
                        typename Inspector::result_type>::type
inspect(Inspector& f, foo& x) {
  int a;
  int b;
  // write back to x at scope exit
  auto g = make_scope_guard([&] {
    x.set_a(a);
    x.set_b(b);
  });
  return f(meta::type_name("foo"), a, b);
}

behavior testee(event_based_actor* self) {
  return {
    [=](const foo& x) { aout(self) << deep_to_string(x) << endl; },
  };
}

class config : public actor_system_config {
public:
  config() {
    add_message_type<foo>("foo");
  }
};

void caf_main(actor_system& system, const config&) {
  anon_send(system.spawn(testee), foo{1, 2});
}

} // namespace

CAF_MAIN()
