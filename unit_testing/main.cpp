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
#include "cppa/anything.hpp"
#include "cppa/detail/demangle.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/process_information.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/task_scheduler.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

// header for experimental stuff
#include "cppa/util/filter_type_list.hpp"
#include "cppa/util/any_tuple_iterator.hpp"

#define CPPA_TEST_CATCH_BLOCK()                                                \
catch (std::exception& e)                                                      \
{                                                                              \
    std::cerr << "test exited after throwing an instance of \""                \
              << ::cppa::detail::demangle(typeid(e).name())                    \
              << "\"\n what(): " << e.what() << std::endl;                     \
    errors += 1;                                                               \
}                                                                              \
catch (...)                                                                    \
{                                                                              \
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

void print_node_id()
{
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

char* find_char_or_end(char* cstr, const char what)
{
    for (char c = *cstr; (c != '\0' && c != what); c = *(++cstr)) { }
    return cstr;
}

std::map<std::string, std::string>
get_kv_pairs(int argc, char** argv, int begin = 1)
{
    std::map<std::string, std::string> result;
    for (int i = begin; i < argc; ++i)
    {
        char* pos = find_char_or_end(argv[i], '=');
        if (*pos == '=')
        {
            char* pos2 = find_char_or_end(pos + 1, '=');
            if (*pos2 == '\0')
            {
                std::pair<std::string, std::string> kvp;
                kvp.first = std::string(argv[i], pos);
                kvp.second = std::string(pos + 1, pos2);
                if (result.insert(std::move(kvp)).second == false)
                {
                    std::string err = "key \"";
                    err += std::string(argv[i], pos);
                    err += "\" already defined";
                    throw std::runtime_error(err);
                }
            }
        }
    }
    return std::move(result);
}

template<typename Iterator, class Container, typename Key>
inline bool found_key(Iterator& i, Container& cont, Key&& key)
{
    return (i = cont.find(std::forward<Key>(key))) != cont.end();
}

int main(int argc, char** argv)
{
    auto args = get_kv_pairs(argc, argv);
    if (!args.empty())
    {
        decltype(args.find("")) i;
        if (found_key(i, args, "run"))
        {
            auto& what = i->second;
            if (what == "remote_actor")
            {
                test__remote_actor(argv[0], true, args);
                return 0;
            }
        }
        else if (found_key(i, args, "scheduler"))
        {
            auto& sched = i->second;
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
            else if (sched == "mock_scheduler")
            {
                cout << "using mock_scheduler" << endl;
                cppa::set_scheduler(new cppa::detail::mock_scheduler);
            }
            else
            {
                cerr << "unknown scheduler: " << sched << endl;
                return 1;
            }
        }
    }
    //print_node_id();
    std::cout << std::boolalpha;
    size_t errors = 0;
    RUN_TEST(test__uniform_type);
    RUN_TEST(test__pattern);
    RUN_TEST(test__yield_interface);
    RUN_TEST(test__ripemd_160);
    RUN_TEST(test__primitive_variant);
    RUN_TEST(test__intrusive_ptr);
    RUN_TEST(test__type_list);
    RUN_TEST(test__tuple);
    RUN_TEST(test__serialization);
    RUN_TEST(test__spawn);
    RUN_TEST(test__local_group);
    RUN_TEST(test__atom);
    RUN_TEST_A3(test__remote_actor, argv[0], false, args);
    cout << endl
         << "error(s) in all tests: " << errors
         << endl;
    return 0;
}
