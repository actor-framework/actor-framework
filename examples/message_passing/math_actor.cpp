/******************************************************************************\
 * This example is a very basic, non-interactive math service implemented     *
 * for both the blocking and the event-based API.                             *
\******************************************************************************/

#include <string>
#include <cassert>
#include <iostream>
#include "cppa/cppa.hpp"

using std::cout;
using std::endl;
using namespace cppa;

// implementation using the blocking API
void blocking_math_fun() {
    bool done = false;
    do_receive (
        // "arg_match" matches the parameter types of given lambda expression
        // thus, it's equal to
        // - on<atom("plus"), int, int>()
        // - on(atom("plus"), val<int>, val<int>)
        on(atom("plus"), arg_match) >> [](int a, int b) {
            reply(atom("result"), a + b);
        },
        on(atom("minus"), arg_match) >> [](int a, int b) {
            reply(atom("result"), a - b);
        },
        on(atom("quit")) >> [&]() {
            // note: this actor uses the blocking API, hence self->quit()
            //       would force stack unwinding by throwing an exception
            done = true;
        }
    ).until(gref(done));
}

// implementation using the event-based API
void math_fun() {
    // execute this behavior until actor terminates
    become (
        on(atom("plus"), arg_match) >> [](int a, int b) {
            reply(atom("result"), a + b);
        },
        on(atom("minus"), arg_match) >> [](int a, int b) {
            reply(atom("result"), a - b);
        },
        // the [=] capture copies the 'this' pointer into the lambda
        // thus, it has access to all members and member functions
        on(atom("quit")) >> [=] {
            // terminate actor with normal exit reason
            self->quit();
        }
    );
}

// utility function
int fetch_result(actor_ptr& calculator, atom_value operation, int a, int b) {
    // send request
    send(calculator, operation, a, b);
    // wait for result
    int result;
    receive(on<atom("result"), int>() >> [&](int r) { result = r; });
    // print and return result
    cout << a << " " << to_string(operation) << " " << b << " = " << result << endl;
    return result;
}

int main() {
    // spawn a context-switching actor that invokes math_fun
    auto a1 = spawn<blocking_api>(blocking_math_fun);
    // spawn an event-based math actor
    auto a2 = spawn(math_fun);
    // do some testing on both implementations
    assert((fetch_result(a1, atom("plus"), 1, 2) == 3));
    assert((fetch_result(a2, atom("plus"), 1, 2) == 3));
    assert((fetch_result(a1, atom("minus"), 2, 1) == 1));
    assert((fetch_result(a2, atom("minus"), 2, 1) == 1));
    // tell both actors to terminate
    send(a1, atom("quit"));
    send(a2, atom("quit"));
    // wait until all spawned actors are terminated
    await_all_others_done();
    // done
    shutdown();
    return 0;
}

