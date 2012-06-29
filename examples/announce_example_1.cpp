#include <vector>
#include <cassert>
#include <utility>
#include <iostream>

#include "cppa/cppa.hpp"

using std::cout;
using std::endl;

using namespace cppa;

// POD struct
struct foo {
    std::vector<int> a;
    int b;
};

// announce requires foo to have the equal operator implemented
bool operator==(const foo& lhs, const foo& rhs) {
    return    lhs.a == rhs.a
           && lhs.b == rhs.b;
}

// a pair of two ints
typedef std::pair<int,int> foo_pair;

// another pair of two ints
typedef std::pair<int,int> foo_pair2;

int main(int, char**) {
    // announces foo to the libcppa type system;
    // the function expects member pointers to all elements of foo
    assert(announce<foo>(&foo::a, &foo::b) == true);

    // announce std::pair<int,int> to the type system;
    // NOTE: foo_pair is NOT distinguishable from foo_pair2!
    assert(announce<foo_pair>(&foo_pair::first, &foo_pair::second) == true);

    // since foo_pair and foo_pair2 are not distinguishable since typedefs
    // do not 'create' an own type, this announce fails, since
    // std::pair<int,int> is already announced
    assert(announce<foo_pair2>(&foo_pair2::first, &foo_pair2::second) == false);

    // send a foo to ourselves
    send(self, foo{std::vector<int>{1, 2, 3, 4}, 5});
    // send a foo_pair2 to ourselves
    send(self, foo_pair2{3, 4});
    // quits the program
    send(self, atom("done"));

    // libcppa returns the same uniform_type_info
    // instance for foo_pair and foo_pair2
    assert(uniform_typeid<foo_pair>() == uniform_typeid<foo_pair2>());

    // receive two messages
    int i = 0;
    receive_for(i, 2) (
        // note: we sent a foo_pair2, but match on foo_pair
        // that's safe because both are aliases for std::pair<int,int>
        on<foo_pair>() >> [](const foo_pair& val) {
            cout << "foo_pair("
                 << val.first << ","
                 << val.second << ")"
                 << endl;
        },
        on<foo>() >> [](const foo& val) {
            cout << "foo({";
            auto i = val.a.begin();
            auto end = val.a.end();
            if (i != end) {
                cout << *i;
                while (++i != end) {
                    cout << "," << *i;
                }
            }
            cout << "}," << val.b << ")" << endl;
        }
    );
    return 0;
}

