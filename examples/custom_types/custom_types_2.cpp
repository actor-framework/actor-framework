// showcases how to add custom message types to CAF
// if friend access for serialization is available

#include <utility>
#include <iostream>

#include "caf/all.hpp"

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

  // compare
  friend bool operator==(const foo& x, const foo& y) {
    return  x.a_ == y.a_ && x.b_ == y.b_;
  }

  // serialize
  template <class T>
  friend void serialize(T& in_or_out, foo& x, const unsigned int) {
    in_or_out & x.a_;
    in_or_out & x.b_;
  }

  // print
  friend std::string to_string(const foo& x) {
    return "foo" + deep_to_string(std::forward_as_tuple(x.a_, x.b_));
  }

private:
  int a_;
  int b_;
};

void testee(event_based_actor* self) {
  self->become (
    [=](const foo& x) {
      aout(self) << to_string(x) << endl;
      self->quit();
    }
  );
}

int main(int, char**) {
  actor_system_config cfg;
  cfg.add_message_type<foo>("foo");
  actor_system system{cfg};
  scoped_actor self{system};
  auto t = self->spawn(testee);
  self->send(t, foo{1, 2});
  self->await_all_other_actors_done();
}
