// Showcases custom message types that cannot provide
// friend access to the inspect() function.

// Manual refs: 57-59, 64-66 (ConfiguringActorApplications)

#include <utility>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::make_pair;

using namespace caf;

namespace {

// identical to our second custom type example,
// but without friend declarations
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

template <class Inspector>
error inspect(Inspector& f, foo& x) {
  // store current state into temporaries, then give the inspector references
  // to temporaries that are written back only when the inspector is saving
  auto a = x.a();
  auto b = x.b();
  auto save = [&]() -> error {
    x.set_a(a);
    x.set_b(b);
    return none;
  };
  return f(meta::type_name("foo"), a, b, meta::save_callback(save));
}

behavior testee(event_based_actor* self) {
  return {
    [=](const foo& x) {
      aout(self) << to_string(x) << endl;
    }
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

} // namespace <anonymous>

CAF_MAIN()
