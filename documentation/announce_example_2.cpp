#include <utility>
#include <iostream>

#include "cppa/cppa.hpp"

using std::cout;
using std::endl;
using std::make_pair;

using namespace cppa;

class foo
{

    int m_a;
    int m_b;

 public:

    foo() : m_a(0), m_b(0) { }

    foo(int a0, int b0) : m_a(a0), m_b(b0) { }

    foo(const foo&) = default;

    foo& operator=(const foo&) = default;

    int a() const { return m_a; }

    void set_a(int val) { m_a = val; }

    int b() const { return m_b; }

    void set_b(int val) { m_b = val; }

};

bool operator==(const foo& lhs, const foo& rhs)
{
    return    lhs.a() == rhs.a()
           && lhs.b() == rhs.b();
}

int main(int, char**)
{
    announce<foo>(make_pair(&foo::a, &foo::set_a),
                  make_pair(&foo::b, &foo::set_b));
    send(self(), foo{1,2});
    receive
    (
        on<foo>() >> [](const foo& val)
        {
            cout << "foo("
                 << val.a() << ","
                 << val.b() << ")"
                 << endl;
        }
    );
    return 0;
}

