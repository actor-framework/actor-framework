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
        on<atom("Get"), actor_ptr>() >> [&](actor_ptr client) {
            send(client, count);
            count = 0;
        },
        on<atom("AddCount"), long>() >> [&](long val) {
            count += val;
        }
    );
}

constexpr long s_val = 100;

void send_range(actor_ptr counter, actor_ptr parent, int from, int to) {
    for (int i = from; i < to; ++i) {
        send(counter, atom("AddCount"), s_val);
    }
    send(parent, atom("Done"));
}

long the_test(int msg_count) {
    actor_ptr self_ptr = self();
    auto counter = spawn(counter_actor);
    for (int i = 1; i < (msg_count / 1000); ++i) {
        spawn(send_range, counter, self_ptr, (i-1)*1000, i*1000);
    }
    auto rule = on<atom("Done"), any_type*>() >> []() { };
    for (int i = 1; i < (msg_count / 1000); ++i) {
        receive(rule);
    }
    send(counter, atom("Get"), self());
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
         << "Throughput = " << (msg_count / elapsed) << " per sec" << endl;
}

int main() {
    run_test(3000000);
    await_all_others_done();
    return 0;
}

