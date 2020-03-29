// showcases how to add custom message types to CAF
// if friend access for serialization is available

#include <utility>
#include <iostream>

#include "caf/all.hpp"

class foo;

CAF_BEGIN_TYPE_ID_BLOCK(custom_types_2, first_custom_type_id)

  CAF_ADD_TYPE_ID(custom_types_2, (foo))

CAF_END_TYPE_ID_BLOCK(custom_types_2)

using std::cout;
using std::endl;
using std::make_pair;

using namespace caf;

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
    [=](const foo& x) {
      aout(self) << to_string(x) << endl;
    }
  };
}

void caf_main(actor_system& sys) {
  anon_send(sys.spawn(testee), foo{1, 2});
}

CAF_MAIN(id_block::custom_types_2)
