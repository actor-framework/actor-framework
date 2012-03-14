#include <string>
#include <iostream>
#include "cppa/cppa.hpp"

using std::cout;
using std::endl;
using namespace cppa;

void math_actor()
{
    // receive messages in an endless loop
    receive_loop
    (
        // "arg_match" matches the parameter types of given lambda expression
        on(atom("plus"), arg_match) >> [](int a, int b)
        {
            reply(atom("result"), a + b);
        },
        on(atom("minus"), arg_match) >> [](int a, int b)
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
        // on<> matches types only, but allows up to four leading atoms
        on<atom("result"), int>() >> [](int result)
        {
            cout << "1 + 2 = " << result << endl;
        }
    );
    send(ma, atom("minus"), 1, 2);
    receive
    (
        // on() matches values; use val<T> to match any value of type T
        on(atom("result"), val<int>) >> [](int result)
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

