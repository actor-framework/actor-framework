#include <utility>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::make_pair;

using namespace caf;

// a simple class using overloaded getter and setter member functions
class foo {

  int m_a;
  int m_b;

 public:

  foo() : m_a(0), m_b(0) { }

  foo(int a0, int b0) : m_a(a0), m_b(b0) { }

  foo(const foo&) = default;

  foo& operator=(const foo&) = default;

  int a() const { return m_a; }

  void a(int val) { m_a = val; }

  int b() const { return m_b; }

  void b(int val) { m_b = val; }

};

// announce requires foo to have the equal operator implemented
bool operator==(const foo& lhs, const foo& rhs) {
  return  lhs.a() == rhs.a()
       && lhs.b() == rhs.b();
}

// a member function pointer to get an attribute of foo
using foo_getter = int (foo::*)() const;
// a member function pointer to set an attribute of foo
using foo_setter = void (foo::*)(int);

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
  // since the member function "a" is ambiguous, the compiler
  // also needs a type to select the correct overload
  foo_getter g1 = &foo::a;
  foo_setter s1 = &foo::a;
  // same is true for b
  foo_getter g2 = &foo::b;
  foo_setter s2 = &foo::b;

  // equal to example 3
  announce<foo>(make_pair(g1, s1), make_pair(g2, s2));

  // alternative syntax that uses casts instead of variables
  // (returns false since foo is already announced)
  announce<foo>(make_pair(static_cast<foo_getter>(&foo::a),
              static_cast<foo_setter>(&foo::a)),
          make_pair(static_cast<foo_getter>(&foo::b),
              static_cast<foo_setter>(&foo::b)));

  // spawn a new testee and send it a foo
  {
    scoped_actor self;
    self->send(spawn(testee), foo{1, 2});
  }
  await_all_actors_done();
  shutdown();
  return 0;
}

