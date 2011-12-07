#include <string>
#include <iostream>
#include "cppa/cppa.hpp"

using namespace cppa;

void math_actor()
{
    receive_loop
    (
        on<atom("plus"), int, int>() >> [](int a, int b)
        {
            reply(atom("result"), a + b);
        },
        on<atom("minus"), int, int>() >> [](int a, int b)
        {
            reply(atom("result"), a - b);
        }
    );
}

int main()
{
    // create a new actor that invokes the function echo_actor
    auto ma = spawn(math_actor);
    send(ma, atom("plus"), 1, 2);
    receive
    (
        on<atom("result"), int>() >> [](int result)
        {
            cout << "1 + 2 = " << result << endl;
        }
    );
    send(ma, atom("minus"), 1, 2);
    receive
    (
        on<atom("result"), int>() >> [](int result)
        {
            cout << "1 - 2 = " << result << endl;
        }
    );
    // force ma to exit
    send(ma, atom(":Exit"), exit_reason::user_defined);
    // wait until ma exited
    await_all_others_done();
    // done
    return 0;
}

