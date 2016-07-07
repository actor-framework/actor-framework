// Showcases custom message types that cannot provide
// friend access to the inspect() function.

// Manual refs: 20-49, 51-76 (TypeInspection)

#include <utility>
#include <iostream>

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

template <class Inspector>
typename std::enable_if<Inspector::is_saving::value,
                        typename Inspector::result_type>::type
inspect(Inspector& f, foo& x) {
  return f(meta::type_name("foo"), x.a(), x.b());
}

template <class Inspector>
typename std::enable_if<Inspector::is_loading::value,
                        typename Inspector::result_type>::type
inspect(Inspector& f, foo& x) {
  struct tmp_t {
    tmp_t(foo& ref) : x_(ref) {
      // nop
    }
    ~tmp_t() {
      // write back to x at scope exit
      x_.set_a(a);
      x_.set_b(b);
    }
    foo& x_;
    int a;
    int b;
  } tmp{x};
  return f(meta::type_name("foo"), tmp.a, tmp.b);
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
