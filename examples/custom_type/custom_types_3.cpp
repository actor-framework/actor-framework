// Showcases custom message types that cannot provide friend access to the
// inspect() function.

#include <iostream>
#include <utility>

#include "caf/all.hpp"

class foo;

CAF_BEGIN_TYPE_ID_BLOCK(custom_types_3, first_custom_type_id)

  CAF_ADD_TYPE_ID(custom_types_3, (foo))

CAF_END_TYPE_ID_BLOCK(custom_types_3)

using std::cout;
using std::endl;
using std::make_pair;

using namespace caf;

// Identical to our second custom type example, but no friend access for
// `inspect`.
// --(rst-foo-begin)--
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
// --(rst-foo-end)--

// --(rst-inspect-begin)--
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, foo& x) {
  auto a = x.a();
  auto b = x.b();
  auto load = meta::load_callback([&]() -> error {
    // Write back to x when loading values from the inspector.
    x.set_a(a);
    x.set_b(b);
    return none;
  });
  return f(meta::type_name("foo"), a, b, load);
}
// --(rst-inspect-end)--

behavior testee(event_based_actor* self) {
  return {
    [=](const foo& x) { aout(self) << to_string(x) << endl; },
  };
}

void caf_main(actor_system& system) {
  anon_send(system.spawn(testee), foo{1, 2});
}

CAF_MAIN(id_block::custom_types_3)
