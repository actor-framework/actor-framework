#include <signal.h>

#include "cppa/cppa.hpp"

using namespace cppa;


/*
 * this example demonstrates how to mix libcppa with unix signals like SIGINT
 * just press ctrl+c (SIGINT) at any time during execution
 *
 * useful for writing daemons using libcppa!
 * 
 * joachim schiele <js@lastlog.de>
 */


void signal_SIG_callback_handler(int signum) {
    std::cout << "signal_SIG_callback_handler received: " << signum << std::endl;
    send(self, atom("done"));
}


// thread mapped actor which emulates a worker thread
void mainActor() {
    bool done = false;
    std::cout << "init mainActor()" << std::endl;

    do_receive (
    on(atom("doWork")) >> []() {
        std::cout << "doWork was called" << std::endl;
        reply(atom("doReply"));
    },
    on(atom("done")) >> [&]() {
        std::cout << "done in mainActor()" << std::endl;
        done = true;
    }
    ).until(gref(done));
}


int main() {
    // thanks to http://www.linuxprogrammingblog.com/code-examples/SIGCHLD-handler
    struct sigaction act;
    act.sa_handler = signal_SIG_callback_handler;
    if (sigaction(SIGINT, &act, 0)) {
        std::cout << "fatal: can't assign a signal to a handler" << std::endl;
        exit(1);
    }

    bool done = false;

    auto mainActor1 = spawn<detached>(mainActor);

    send(mainActor1, atom("doWork"));

    do_receive (
        // invoke this lambda expression if we receive a string
    on(atom("doReply")) >> []() {
        std::cout << "doReply was called; " << std::endl;
        std::cout << "-- you can now use CTRL+c (SIGINT) to stop execution gracefully --" << std::endl;
    },
    on(atom("done")) >> [&]() {
        done = true;
    }
    ).until(gref(done));

    std::cout << "exiting main do_receive loop and sending 'done' to the mainActor1 instance" << std::endl;

    send(mainActor1, atom("done"));

    // wait until all other actors we've spawned are done
    await_all_others_done();

    std::cout << "mainActor1 is now gone!" << std::endl;

    // done
    return 0;
}

