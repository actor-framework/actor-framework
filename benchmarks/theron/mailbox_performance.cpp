#define THERON_USE_BOOST_THREADS 1
#include <Theron/Framework.h>
#include <Theron/Actor.h>

#include <thread>
#include <cstdint>
#include <iostream>
#include <functional>

#include "utility.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::int64_t;

using namespace Theron;

int64_t t_max = 0;

struct receiver : Actor {

    int64_t m_num;

    void handler(const int64_t&, const Address from) {
        if (++m_num == t_max)
            Send(t_max, from);
    }

    receiver() : m_num(0) {
        RegisterHandler(this, &receiver::handler);
    }

};

void send_sender(Framework& f, ActorRef ref, Address waiter, int64_t num) {
    auto addr = ref.GetAddress();
    int64_t msg;
    for (int64_t i = 0; i < num; ++i) f.Send(msg, waiter, addr);
}

void push_sender(Framework& f, ActorRef ref, Address waiter, int64_t num) {
    int64_t msg;
    for (int64_t i = 0; i < num; ++i) ref.Push(msg, waiter);
}

void usage() {
    cout << "usage ('push'|'send') (num_threads) (num_messages)" << endl;
    exit(1);
}

int main(int argc, char** argv) {
    if (argc != 4) {
        usage();
    }
    enum { invalid_impl, push_impl, send_impl } impl = invalid_impl;
    if (strcmp(argv[1], "push") == 0) impl = push_impl;
    else if (strcmp(argv[1], "send") == 0) impl = send_impl;
    else usage();
    auto num_sender = rd<int64_t>(argv[2]);
    auto num_msgs = rd<int64_t>(argv[3]);
    Receiver r;
    t_max = num_sender * num_msgs;
    auto receiverAddr = r.GetAddress();
    Framework framework(num_cores());
    ActorRef aref(framework.CreateActor<receiver>());
    std::list<std::thread> threads;
    auto impl_fun = (impl == push_impl) ? send_sender : push_sender;
    for (int64_t i = 0; i < num_sender; ++i) {
        threads.push_back(std::thread(impl_fun, std::ref(framework), aref, receiverAddr, num_msgs));
    }
    r.Wait();
    for (auto& t : threads) t.join();
    return 0;
}

