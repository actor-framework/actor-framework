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
#include "hash_of.hpp"

#include "cppa/tuple.hpp"
#include "cppa/config.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/process_information.hpp"
#include "cppa/detail/task_scheduler.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

#define RUN_TEST(fun_name)                                                     \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
errors += fun_name ();                                                         \
std::cout << std::endl

#define RUN_TEST_A1(fun_name, arg1)                                            \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
errors += fun_name ((arg1));                                                   \
std::cout << std::endl

#define RUN_TEST_A2(fun_name, arg1, arg2)                                      \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
errors += fun_name ((arg1), (arg2));                                           \
std::cout << std::endl

#define RUN_TEST_A3(fun_name, arg1, arg2, arg3)                                \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
errors += fun_name ((arg1), (arg2), (arg3));                                   \
std::cout << std::endl

using std::cout;
using std::cerr;
using std::endl;

void print_node_id()
{
    const auto& pinfo = cppa::process_information::get();
    auto node_id_hash = pinfo.node_id_as_string();
    cout << "node id: " << node_id_hash << endl;
    cout << "process id: " << pinfo.process_id << endl;
    cout << "actor id format: {process id}.{actor id}@{node id}" << endl;
    cout << "example actor id: " << pinfo.process_id
                                 << ".42@"
                                 << node_id_hash
                                 << endl;
}

int main(int argc, char** c_argv)
{
    std::string sched_option = "scheduler=";
    std::vector<std::string> argv;
    for (int i = 1; i < argc; ++i)
    {
        argv.push_back(c_argv[i]);
    }
    if (!argv.empty())
    {
        if (argv.size() == 1 && argv[0] == "performance_test")
        {
            cout << endl << "run queue performance test ... " << endl;
            test__queue_performance();
            return 0;
        }
        else if (argv.size() == 2 && argv[0] == "test__remote_actor")
        {
            test__remote_actor(c_argv[0], true, argv);
            return 0;
        }
        else if (   argv.size() == 1
                 && argv[0].compare(0, sched_option.size(), sched_option) == 0)
        {
            std::string sched = argv[0].substr(sched_option.size());
            if (sched == "task_scheduler")
            {
                cout << "using task_scheduler" << endl;
                cppa::set_scheduler(new cppa::detail::task_scheduler);
            }
            else if (sched == "thread_pool_scheduler")
            {
                cout << "using thread_pool_scheduler" << endl;
                cppa::set_scheduler(new cppa::detail::thread_pool_scheduler);
            }
            else
            {
                cerr << "unknown scheduler: " << sched << endl;
                return 1;
            }
        }
        else
        {
            cerr << "usage: test [performance_test]" << endl;
            return 1;
        }
    }
    print_node_id();
    cout << endl << endl;
    std::cout << std::boolalpha;
    size_t errors = 0;
    RUN_TEST(test__ripemd_160);
    RUN_TEST(test__primitive_variant);
    RUN_TEST(test__uniform_type);
    RUN_TEST(test__intrusive_ptr);
    RUN_TEST(test__a_matches_b);
    RUN_TEST(test__type_list);
    RUN_TEST(test__tuple);
    RUN_TEST(test__serialization);
    RUN_TEST(test__spawn);
    RUN_TEST(test__local_group);
    RUN_TEST(test__atom);
    RUN_TEST_A3(test__remote_actor, c_argv[0], false, argv);
    cout << endl
         << "error(s) in all tests: " << errors
         << endl;
    return 0;
}
