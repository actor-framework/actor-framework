#include <map>
#include <cstdio>
#include <atomic>
#include <string>
#include <limits>
#include <vector>
#include <cstring>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <typeinfo>
#include <iostream>

#include "test.hpp"
#include "ping_pong.hpp"

#include "cppa/cppa.hpp"
#include "cppa/match.hpp"
#include "cppa/config.hpp"
#include "cppa/anything.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/detail/demangle.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/process_information.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

#define CPPA_TEST_CATCH_BLOCK()                                                \
catch (std::exception& e) {                                                    \
    std::cerr << "test exited after throwing an instance of \""                \
              << ::cppa::detail::demangle(typeid(e).name())                    \
              << "\"\n what(): " << e.what() << std::endl;                     \
    errors += 1;                                                               \
}                                                                              \
catch (...) {                                                                  \
    std::cerr << "test exited because of an unknown exception" << std::endl;   \
    errors += 1;                                                               \
}

#define RUN_TEST(fun_name)                                                     \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
try { errors += fun_name (); } CPPA_TEST_CATCH_BLOCK()                         \
std::cout << std::endl

#define RUN_TEST_A1(fun_name, arg1)                                            \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
try { errors += fun_name ((arg1)); } CPPA_TEST_CATCH_BLOCK()                   \
std::cout << std::endl

#define RUN_TEST_A2(fun_name, arg1, arg2)                                      \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
try { errors += fun_name ((arg1), (arg2)); } CPPA_TEST_CATCH_BLOCK()           \
std::cout << std::endl

#define RUN_TEST_A3(fun_name, arg1, arg2, arg3)                                \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
try { errors += fun_name ((arg1), (arg2), (arg3));  } CPPA_TEST_CATCH_BLOCK()  \
std::cout << std::endl

using std::cout;
using std::cerr;
using std::endl;

using namespace cppa;

std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream strs{str};
    std::string tmp;
    while (std::getline(strs, tmp, delim)) result.push_back(tmp);
    return result;
}

void print_node_id() {
    auto pinfo = cppa::process_information::get();
    auto node_id_hash = cppa::to_string(pinfo->node_id());
    cout << "node id: " << node_id_hash << endl;
    cout << "process id: " << pinfo->process_id() << endl;
    cout << "actor id format: {process id}.{actor id}@{node id}" << endl;
    cout << "example actor id: " << pinfo->process_id()
                                 << ".42@"
                                 << node_id_hash
                                 << endl;
}

std::vector<string_pair> get_kv_pairs(int argc, char** argv, int begin = 1) {
    std::vector<string_pair> result;
    for (int i = begin; i < argc; ++i) {
        auto vec = split(argv[i], '=');
        if (vec.size() != 2) {
            cerr << "\"" << argv[i] << "\" is not a key-value pair" << endl;
        }
        else if (std::any_of(result.begin(), result.end(),
                             [&](const string_pair& p) { return p.first == vec[0]; })) {
            cerr << "key \"" << vec[0] << "\" is already defined" << endl;
        }
        else {
            result.emplace_back(vec[0], vec[1]);
        }
    }
    return result;
}

void usage(const char* argv0) {
    cout << "usage: " << split(argv0, '/').back() << " "
         << "[run=remote_actor] "
         << "[scheduler=(thread_pool_scheduler|mock_scheduler)]"
         << endl;
}

int main(int argc, char** argv) {
    auto args = get_kv_pairs(argc, argv);
    match_each(args) (
        on("run", "remote_actor") >> [&]() {
            test__remote_actor(argv[0], true, args);
            exit(0);
        },
        on("run", "threaded_ping_pong") >> []() {
            spawn<detached>(pong, spawn<detached>(ping, 1000));
            await_all_others_done();
            exit(0);
        },
        on("run", "ping_pong") >> []() {
            spawn_event_based_pong(spawn_event_based_ping(1000000));
            await_all_others_done();
            exit(0);
        },
        on("run_ping", arg_match) >> [&](const std::string& num_pings) {
            auto ping_actor = spawn<detached>(ping, std::stoi(num_pings));
            //auto ping_actor = spawn_event_based_ping(std::stoi(num_pings));
            std::uint16_t port = 4242;
            bool success = false;
            do {
                try {
                    publish(ping_actor, port);
                    success = true;
                }
                catch (bind_failure&) {
                    // try next port
                    ++port;
                }
            }
            while (!success);
            cout << "port is " << port << endl;
            await_all_others_done();
            exit(0);
        },
        on("scheduler", val<std::string>) >> [](const std::string& sched) {
            if (sched == "thread_pool_scheduler") {
                cout << "using thread_pool_scheduler" << endl;
                set_scheduler(new cppa::detail::thread_pool_scheduler);
            }
            else {
                cerr << "unknown scheduler: " << sched << endl;
                exit(1);
            }
        }
    );
    std::cout << std::boolalpha;
    size_t errors = 0;
    //print_node_id();
    RUN_TEST(test__ripemd_160);
    RUN_TEST(test__primitive_variant);
    RUN_TEST(test__intrusive_containers);
    RUN_TEST(test__uniform_type);
    RUN_TEST(test__pattern);
    RUN_TEST(test__match);
    RUN_TEST(test__intrusive_ptr);
    RUN_TEST(test__type_list);
    RUN_TEST(test__fixed_vector);
    RUN_TEST(test__tuple);
    RUN_TEST(test__serialization);
    RUN_TEST(test__atom);
    RUN_TEST(test__yield_interface);
    RUN_TEST(test__spawn);
    RUN_TEST(test__local_group);
    RUN_TEST_A3(test__remote_actor, argv[0], false, args);
    cout << endl
         << "error(s) in all tests: " << errors
         << endl;
    return 0;
}
