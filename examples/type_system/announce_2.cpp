#include <utility>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::make_pair;

using namespace caf;

// a simple class using getter and setter member functions
class foo {

  int m_a;
  int m_b;

 public:

  foo(int a0 = 0, int b0 = 0) : m_a(a0), m_b(b0) { }

  foo(const foo&) = default;

  foo& operator=(const foo&) = default;

  int a() const { return m_a; }

  void set_a(int val) { m_a = val; }

  int b() const { return m_b; }

  void set_b(int val) { m_b = val; }

};

// announce requires foo to be comparable
bool operator==(const foo& lhs, const foo& rhs) {
  return  lhs.a() == rhs.a()
       && lhs.b() == rhs.b();
}

void testee(event_based_actor* self) {
  self->become (
    on<foo>() >> [=](const foo& val) {
      aout(self) << "foo("
             << val.a() << ", "
             << val.b() << ")"
             << endl;
      self->quit();
    }
  );
}

int main(int, char**) {
  // if a class uses getter and setter member functions,
  // we pass those to the announce function as { getter, setter } pairs.
  announce<foo>(make_pair(&foo::a, &foo::set_a),
          make_pair(&foo::b, &foo::set_b));
  {
    scoped_actor self;
    auto t = spawn(testee);
    self->send(t, foo{1, 2});
  }
  await_all_actors_done();
  shutdown();
  return 0;
}
