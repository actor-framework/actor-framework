// Showcases custom message types that cannot provide
// friend access to the inspect() function.

#include "caf/all.hpp"

#include <iostream>
#include <utility>

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
bool inspect(Inspector& f, foo& x) {
  auto get_a = [&x] { return x.a(); };
  auto set_a = [&x](int val) {
    x.set_a(val);
    return true;
  };
  auto get_b = [&x] { return x.b(); };
  auto set_b = [&x](int val) {
    x.set_b(val);
    return true;
  };
  return f.object(x).fields(f.field("a", get_a, set_a),
                            f.field("b", get_b, set_b));
}
// --(rst-inspect-end)--

behavior testee(event_based_actor* self) {
  return {
    [self](const foo& x) { aout(self).println("{}", x); },
  };
}

void caf_main(actor_system& system) {
  anon_mail(foo{1, 2}).send(system.spawn(testee));
}

CAF_MAIN(id_block::custom_types_3)
