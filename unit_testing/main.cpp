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

#define RUN_TEST(fun_name)                                                     \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
errors += fun_name ();                                                         \
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
    print_node_id();
    cout << endl << endl;
    //return 0;

    std::vector<std::string> argv;
    for (int i = 1; i < argc; ++i)
    {
        argv.push_back(c_argv[i]);
    }
    if (!argv.empty())
    {
        if (argv.size() == 1 && argv.front() == "performance_test")
        {
            cout << endl << "run queue performance test ... " << endl;
            test__queue_performance();
            return 0;
        }
        else
        {
            cerr << "usage: test [performance_test]" << endl;
        }
    }
    else
    {
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
        cout << endl
             << "error(s) in all tests: " << errors
             << endl;
    }
    return 0;
}
