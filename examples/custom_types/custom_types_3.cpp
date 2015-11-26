// showcases how to add custom message types to CAF
// if no friend access for serialization is possible

#include <utility>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::make_pair;

using namespace caf;

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

// comparing is straightforward ...
bool operator==(const foo& x, const foo& y) {
  return x.a() == y.a() && x.b() == y.b();
}

// ... and so is to_string, ...
std::string to_string(const foo& x) {
  return "foo" + deep_to_string(std::forward_as_tuple(x.a(), x.b()));
}

// ... but we need to split serialization into a saving ...
template <class T>
typename std::enable_if<T::is_saving::value>::type
serialize(T& out, const foo& x, const unsigned int) {
  out << x.a() << x.b();
}

// ... and a loading function
template <class T>
typename std::enable_if<T::is_loading::value>::type
serialize(T& in, foo& x, const unsigned int) {
  int tmp;
  in >> tmp;
  x.set_a(tmp);
  in >> tmp;
  x.set_b(tmp);
}

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
