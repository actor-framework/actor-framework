#include <iostream>
#include "cppa/cppa.hpp"
#include <boost/progress.hpp>

using std::cout;
using std::endl;
using boost::timer;
using namespace cppa;

void counter_actor() {
    long count = 0;
    receive_loop (
        on<atom("AddCount"), long>() >> [&](long val) {
            count += val;
        },
        on<atom("Get"), actor_ptr>() >> [&](actor_ptr client) {
            send(client, count);
            count = 0;
        }
    );
}

long the_test(int msg_count) {
    constexpr long val = 100;
    auto counter = spawn(counter_actor);
    auto msg = make_tuple(atom("AddCount"), val);
    for (int i = 0; i < msg_count; ++i) {
        counter << msg;
        //send(counter, atom("AddCount"), val);
    }
    send(counter, atom("Get"), self);
    long result = 0;
    receive (
        on<long>() >> [&](long value) {
            result = value;
        }
    );
    send(counter, atom(":Exit"), exit_reason::user_defined);
    return result;
}

void run_test(int msg_count) {
    timer t0;
    long count = the_test(msg_count);
    auto elapsed = t0.elapsed();
    cout << "Count is " << count << endl
         << "Test took " << elapsed << " seconds" << endl
         << "Throughput = " << static_cast<unsigned long>(msg_count / elapsed)
                            << " per sec" << endl;
}

int main() {
    run_test(3000000);
    await_all_others_done();
    return 0;
}

