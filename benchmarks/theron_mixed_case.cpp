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

constexpr uint64_t s_task_n = uint64_t(86028157)*329545133;
constexpr uint64_t s_factor1 = 86028157;
constexpr uint64_t s_factor2 = 329545133;

typedef std::vector<uint64_t> factors;

struct calc_msg { uint64_t value; };
struct result_msg { factors result; };
struct token_msg { int value; };
struct init_msg { int ring_size; int token_value; int iterations; };
struct master_done { };
struct worker_done { };


struct worker : Actor {
    void handle_calc(const calc_msg& msg, Address from) {
        factorize(msg.value);
    }
    void handle_master_done(const master_done&, Address from) {
        Send(worker_done(), from);
    }
    worker() {
        RegisterHandler(this, &worker::handle_calc);
        RegisterHandler(this, &worker::handle_master_done);
    }

};

struct chain_link : Actor {
    Address next;
    void handle_token(const token_msg& msg, Address) {
        Send(msg, next);
    }
    typedef struct { Address next; } Parameters;
    chain_link(const Parameters& p) : next(p.next) {
        RegisterHandler(this, &chain_link::handle_token);
    }
};

struct master : Actor {
    Address mc;
    int iteration;
    int max_iterations;
    Address next;
    ActorRef w;
    int ring_size;
    int initial_token_value;
    std::vector<ActorRef> m_children;
    void new_ring() {
        m_children.clear();
        w.Push(calc_msg{s_task_n}, GetAddress());
        next = GetAddress();
        for (int i = 1; i < ring_size; ++i) {
            m_children.push_back(GetFramework().CreateActor<chain_link>(chain_link::Parameters{next}));
            next = m_children.back().GetAddress();
        }
        Send(token_msg{initial_token_value}, next);
    }
    void handle_init(const init_msg& msg, Address) {
        w = GetFramework().CreateActor<worker>();
        iteration = 0;
        ring_size = msg.ring_size;
        initial_token_value = msg.token_value;
        max_iterations = msg.iterations;
        new_ring();
    }
    void handle_token(const token_msg& msg, Address) {
        if (msg.value == 0) {
            if (++iteration < max_iterations) {
                new_ring();
            }
            else {
                w.Push(master_done(), GetAddress());
            }
        }
        else {
            Send(token_msg{msg.value - 1}, next);
        }
    }
    void handle_worker_done(const worker_done&, Address) {
        Send(master_done(), mc);
        w = ActorRef::Null();
    }
    typedef struct { Address mc; } Parameters;
    master(const Parameters& p) : mc(p.mc), iteration(0) {
        RegisterHandler(this, &master::handle_init);
        RegisterHandler(this, &master::handle_token);
        RegisterHandler(this, &master::handle_worker_done);
    }
};

void usage() {
    cout << "usage: mailbox_performance "
            "'send' (num rings) (ring size) "
            "(initial token value) (repetitions)"
         << endl
         << endl;
    exit(1);
}

int main(int argc, char** argv) {
    if (argc != 6) usage();
    if (strcmp("send", argv[1]) != 0) usage();
    int num_rings = rd<int>(argv[2]);
    int ring_size = rd<int>(argv[3]);
    int inital_token_value = rd<int>(argv[4]);
    int repetitions = rd<int>(argv[5]);
    Receiver r;
    Framework framework(num_cores());
    std::vector<ActorRef> masters;
    for (int i = 0; i < num_rings; ++i) {
        masters.push_back(framework.CreateActor<master>(master::Parameters{r.GetAddress()}));
    }
    for (ActorRef& m : masters) {
        m.Push(init_msg{ring_size, inital_token_value, repetitions}, r.GetAddress());
    }
    for (int i = 0; i < num_rings; ++i) r.Wait();
    return 0;
}
