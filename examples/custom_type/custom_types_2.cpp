// showcases how to add custom message types to CAF
// if friend access for serialization is available

#include <iostream>
#include <utility>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::make_pair;

using namespace caf;

namespace {

// a simple class using getter and setter member functions
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

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f, foo& x) {
    return f(meta::type_name("foo"), x.a_, x.b_);
  }

private:
  int a_;
  int b_;
};

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
