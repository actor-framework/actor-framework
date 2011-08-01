#include <utility>
#include <iostream>

#include "cppa/cppa.hpp"

using std::cout;
using std::endl;

using namespace cppa;

struct foo
{
    int a;
    int b;
};

bool operator==(const foo& lhs, const foo& rhs)
{
    return    lhs.a == rhs.a
           && lhs.b == rhs.b;
}

typedef std::pair<int,int> foo_pair;

int main(int, char**)
{
    announce<foo>(&foo::a, &foo::b);
    announce<foo_pair>(&foo_pair::first,
                       &foo_pair::second);
    send(self(), foo{1,2});
    send(self(), foo_pair{3,4});
    send(self(), atom("done"));
    receive_loop
    (
        on<atom("done")>() >> []()
        {
            exit(0);
        },
        on<foo_pair>() >> [](const foo_pair& val)
        {
            cout << "foo_pair("
                 << val.first << ","
                 << val.second << ")"
                 << endl;
        },
        on<foo>() >> [](const foo& val)
        {
            cout << "foo("
                 << val.a << ","
                 << val.b << ")"
                 << endl;
        }
    );
    return 0;
}

