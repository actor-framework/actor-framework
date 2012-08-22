#define THERON_USE_BOOST_THREADS 1
#include <Theron/Framework.h>
#include <Theron/Actor.h>

#include <thread>
#include <cstdint>
#include <iostream>
#include <functional>

#include "utility.hpp"

using std::cout;
using std::endl;
using std::uint32_t;

struct spread { int value; };
struct result { uint32_t value; };

using namespace Theron;

struct testee : Actor {

    Address m_parent;
    bool m_first_result_received;
    uint32_t m_first_result;
    std::vector<ActorRef> m_children;

    void spread_handler(const spread& arg, const Address) {
        if (arg.value == 0) {
            Send(result{1}, m_parent);
        }
        else {
            spread msg = {arg.value-1};
            Parameters params = {GetAddress()};
            for (int i = 0; i < 2; ++i) {
                m_children.push_back(GetFramework().CreateActor<testee>(params));
                m_children.back().Push(msg, GetAddress());
            }
        }
    }

    void result_handler(const result& arg, const Address) {
        if (!m_first_result_received) {
            m_first_result_received = true;
            m_first_result = arg.value;
        }
        else {
            m_children.clear();
            Send(result{m_first_result + arg.value}, m_parent);
        }
    }

    typedef struct { Address arg0; } Parameters;

    testee(const Parameters& p) : m_parent(p.arg0), m_first_result_received(false) {
        RegisterHandler(this, &testee::spread_handler);
        RegisterHandler(this, &testee::result_handler);
    }

};

void usage() {
    cout << "usage: theron_actor_creation _ POW" << endl
         << "       creates 2^POW actors" << endl
         << endl;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        usage();
        return 1;
    }
    int num = rd<int>(argv[2]);
    Receiver r;
    Framework framework(num_cores());
    ActorRef aref(framework.CreateActor<testee>(testee::Parameters{r.GetAddress()}));
    aref.Push(spread{num}, r.GetAddress());
    r.Wait();
}

