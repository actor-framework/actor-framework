#include <vector>
#include <cassert>
#include <utility>
#include <iostream>

#include "caf/all.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"

using std::cout;
using std::endl;
using std::vector;

using namespace caf;

// POD struct
struct foo {
  std::vector<int> a;
  int b;
};

// announce requires foo to have the equal operator implemented
bool operator==(const foo& lhs, const foo& rhs) {
  return  lhs.a == rhs.a
       && lhs.b == rhs.b;
}

// a pair of two ints
using foo_pair = std::pair<int, int>;

// another pair of two ints
using foo_pair2 = std::pair<int, int>;

// a struct with member vector<vector<...>>
struct foo2 {
  int a;
  vector<vector<double>> b;
};

bool operator==( const foo2& lhs, const foo2& rhs ) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

// receives `remaining` messages
void testee(event_based_actor* self, size_t remaining) {
  auto set_next_behavior = [=] {
    if (remaining > 1) testee(self, remaining - 1);
    else self->quit();
  };
  self->become (
    // note: we sent a foo_pair2, but match on foo_pair
    // that's safe because both are aliases for std::pair<int, int>
    on<foo_pair>() >> [=](const foo_pair& val) {
      cout << "foo_pair("
         << val.first << ", "
         << val.second << ")"
         << endl;
      set_next_behavior();
    },
    on<foo>() >> [=](const foo& val) {
      cout << "foo({";
      auto i = val.a.begin();
      auto end = val.a.end();
      if (i != end) {
        cout << *i;
        while (++i != end) {
          cout << ", " << *i;
        }
      }
      cout << "}, " << val.b << ")" << endl;
      set_next_behavior();
    }
  );
}

int main(int, char**) {

  // announces foo to the libcaf type system;
  // the function expects member pointers to all elements of foo
  announce<foo>(&foo::a, &foo::b);

  // announce foo2 to the libcaf type system,
  // note that recursive containers are managed automatically by libcaf
  announce<foo2>(&foo2::a, &foo2::b);

  foo2 vd;
  vd.a = 5;
  vd.b.resize(1);
  vd.b.back().push_back(42);

  vector<char> buf;
  binary_serializer bs(std::back_inserter(buf));
  bs << vd;

  binary_deserializer bd(buf.data(), buf.size());
  foo2 vd2;
  uniform_typeid<foo2>()->deserialize(&vd2, &bd);

  assert(vd == vd2);

  // announce std::pair<int, int> to the type system;
  // NOTE: foo_pair is NOT distinguishable from foo_pair2!
  auto uti = announce<foo_pair>(&foo_pair::first, &foo_pair::second);

  // since foo_pair and foo_pair2 are not distinguishable since typedefs
  // do not 'create' an own type, this announce fails, since
  // std::pair<int, int> is already announced
  assert(announce<foo_pair2>(&foo_pair2::first, &foo_pair2::second) == uti);

  // libcaf returns the same uniform_type_info
  // instance for foo_pair and foo_pair2
  assert(uniform_typeid<foo_pair>() == uniform_typeid<foo_pair2>());

  // spawn a testee that receives two messages
  auto t = spawn(testee, 2);
  {
    scoped_actor self;
    // send t a foo
    self->send(t, foo{std::vector<int>{1, 2, 3, 4}, 5});
    // send t a foo_pair2
    self->send(t, foo_pair2{3, 4});
  }
  await_all_actors_done();
  shutdown();
  return 0;
}

