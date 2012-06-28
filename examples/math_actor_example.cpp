#include <string>
#include <cassert>
#include <iostream>
#include "cppa/cppa.hpp"

using std::cout;
using std::endl;
using namespace cppa;

void math_fun() {
    bool done = false;
    do_receive (
        // "arg_match" matches the parameter types of given lambda expression
        // thus, it's equal to
        // - on<atom("plus"), int, int>()
        // - on(atom("plus"), val<int>, val<int>)
        on(atom("plus"), arg_match) >> [](int a, int b) {
            reply(atom("result"), a + b);
        },
        on<atom("minus"), int, int>() >> [](int a, int b) {
            reply(atom("result"), a - b);
        },
        on(atom("quit")) >> [&]() {
            // note: quit(exit_reason::normal) would terminate the actor
            //       but is best avoided since it forces stack unwinding
            //       by throwing an exception
            done = true;
        }
    )
    .until(gref(done));
}

struct math_actor : event_based_actor {
    void init() {
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
            on(atom("quit")) >> [=]() {
                // set an empty behavior
                // (terminates actor with normal exit reason)
                quit();
            }
        );
    }
};

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
    auto a1 = spawn(math_fun);
    // spawn an event-based math actor
    auto a2 = spawn<math_actor>();
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
    return 0;
}

