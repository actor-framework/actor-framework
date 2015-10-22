#include <iostream>
#include "caf/all.hpp"
#include "caf/to_string.hpp"

using std::cout;
using std::endl;
using std::make_pair;

using namespace caf;

// the foo class from example 3
class foo {

  int a_;
  int b_;

public:

  foo() : a_(0), b_(0) { }

  foo(int a0, int b0) : a_(a0), b_(b0) { }

  foo(const foo&) = default;

  foo& operator=(const foo&) = default;

  int a() const { return a_; }

  void set_a(int val) { a_ = val; }

  int b() const { return b_; }

  void set_b(int val) { b_ = val; }

};

// needed for operator==() of bar
bool operator==(const foo& lhs, const foo& rhs) {
  return  lhs.a() == rhs.a()
       && lhs.b() == rhs.b();
}

// simple struct that has foo as a member
struct bar {
  foo f;
  int i;
};

// announce requires bar to have the equal operator implemented
bool operator==(const bar& lhs, const bar& rhs) {
  return  lhs.f == rhs.f
       && lhs.i == rhs.i;
}

// "worst case" class ... not a good software design at all ;)
class baz {

  foo f_;

public:

  bar b;

  // announce requires a default constructor
  baz() = default;

  inline baz(const foo& mf, const bar& mb) : f_(mf), b(mb) { }

  const foo& f() const { return f_; }

  void set_f(const foo& val) { f_ = val; }

};

// even worst case classes have to implement operator==
bool operator==(const baz& lhs, const baz& rhs) {
  return  lhs.f() == rhs.f()
       && lhs.b == rhs.b;
}

// receives `remaining` messages
void testee(event_based_actor* self, size_t remaining) {
  auto set_next_behavior = [=] {
    if (remaining > 1) testee(self, remaining - 1);
    else self->quit();
  };
  self->become (
    [=](const bar& val) {
      aout(self) << "bar(foo("
             << val.f.a() << ", "
             << val.f.b() << "), "
             << val.i << ")"
             << endl;
      set_next_behavior();
    },
    [=](const baz& val) {
      // prints: baz ( foo ( 1, 2 ), bar ( foo ( 3, 4 ), 5 ) )
      aout(self) << to_string(make_message(val)) << endl;
      set_next_behavior();
    }
  );
}

int main(int, char**) {
  // bar has a non-trivial data member f, thus, we have to told
  // announce how to serialize/deserialize this member;
  // this is was the compound_member function is for;
  // it takes a pointer to the non-trivial member as first argument
  // followed by all "sub-members" either as member pointer or
  // { getter, setter } pair
  auto meta_bar_f = [] {
    return compound_member(&bar::f,
                           make_pair(&foo::a, &foo::set_a),
                           make_pair(&foo::b, &foo::set_b));
  };
  // with meta_bar_f, we can now announce bar
  announce<bar>("bar", meta_bar_f(), &bar::i);
  // baz has non-trivial data members with getter/setter pair
  // and getter returning a mutable reference
  announce<baz>("baz", compound_member(make_pair(&baz::f, &baz::set_f),
                                       make_pair(&foo::a, &foo::set_a),
                                       make_pair(&foo::b, &foo::set_b)),
                // compound member that has a compound member
                compound_member(&baz::b, meta_bar_f(), &bar::i));
  // spawn a testee that receives two messages
  auto t = spawn(testee, size_t{2});
  {
    scoped_actor self;
    self->send(t, bar{foo{1, 2}, 3});
    self->send(t, baz{foo{1, 2}, bar{foo{3, 4}, 5}});
  }
  await_all_actors_done();
  shutdown();
  return 0;
}

