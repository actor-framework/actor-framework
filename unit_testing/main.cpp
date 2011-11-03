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
#include "cppa/detail/demangle.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/process_information.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/task_scheduler.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"


// header for experimental stuff
#include "cppa/util/filter_type_list.hpp"
#include "cppa/util/any_tuple_iterator.hpp"
#include "cppa/detail/matcher_arguments.hpp"

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




namespace {
using namespace cppa;

struct wildcard { };

typedef std::vector<const uniform_type_info*> uti_vec;

template<typename T>
struct is_wildcard { static constexpr bool value = false; };

template<>
struct is_wildcard<wildcard> { static constexpr bool value = true; };

template<typename OutputIterator, typename T0>
void fill_vec(OutputIterator& ii)
{
    *ii = is_wildcard<T0>::value ? nullptr : cppa::uniform_typeid<T0>();
}

template<typename OutputIterator, typename T0, typename T1, typename... Tn>
void fill_vec(OutputIterator& ii);

template<typename OutputIterator, typename T0>
struct fill_vec_util
{
    template<typename T1, typename... Tn>
    static void _(OutputIterator& ii)
    {
        *ii = cppa::uniform_typeid<T0>();
        ++ii;
        fill_vec<OutputIterator,T1,Tn...>(ii);
    }
};

template<typename OutputIterator>
struct fill_vec_util<OutputIterator, wildcard>
{
    template<typename T1, typename... Tn>
    static void _(OutputIterator& ii)
    {
        *ii = nullptr;
        ++ii;
        fill_vec<OutputIterator,T1,Tn...>(ii);
    }
};


template<typename OutputIterator, typename T0, typename T1, typename... Tn>
void fill_vec(OutputIterator& ii)
{
    fill_vec_util<OutputIterator,T0>::template _<T1,Tn...>(ii);
}

bool match_fun(const cppa::uniform_type_info** utis, size_t utis_size,
               const cppa::any_tuple& msg)
{
    if (msg.size() == utis_size)
    {
        for (size_t i = 0; i < utis_size; ++i)
        {
            if (utis[i] != &(msg.utype_info_at(i))) return false;
        }
        return true;
    }
    return false;

}

bool compare_at(size_t pos,
                const cppa::any_tuple& msg,
                const cppa::uniform_type_info* uti,
                const void* value)
{
    if (uti == &(msg.utype_info_at(pos)))
    {
        return (value) ? uti->equal(msg.at(pos), value) : true;
    }
    return false;
}

template<typename T>
struct tdata_from_type_list;

template<typename... T>
struct tdata_from_type_list<cppa::util::type_list<T...>>
{
    typedef cppa::detail::tdata<T...> type;
};

struct matcher_types_args
{

    size_t m_pos;
    size_t m_size;
    void const* const* m_data;
    cppa::uniform_type_info const* const* m_types;

 public:

    matcher_types_args(size_t msize,
                       void const* const* mdata,
                       cppa::uniform_type_info const* const* mtypes)
        : m_pos(0)
        , m_size(msize)
        , m_data(mdata)
        , m_types(mtypes)
    {
    }

    matcher_types_args(const matcher_types_args&) = default;

    matcher_types_args& operator=(const matcher_types_args&) = default;

    inline bool at_end() const { return m_pos == m_size; }

    inline matcher_types_args& next()
    {
        ++m_pos;
        return *this;
    }

    inline const uniform_type_info* type() const
    {
        return m_types[m_pos];
    }

    inline bool has_value() const { return value() != nullptr; }

    inline const void* value() const
    {
        return m_data[m_pos];
    }

};

struct matcher_tuple_args
{

    util::any_tuple_iterator iter;
    std::vector<size_t>* mapping;

    matcher_tuple_args(const any_tuple& tup,
                       std::vector<size_t>* mv = 0)
        : iter(tup), mapping(mv)
    {
    }

    matcher_tuple_args(const util::any_tuple_iterator& from_iter,
                       std::vector<size_t>* mv = nullptr)
        : iter(from_iter), mapping(mv)
    {
    }

    inline bool at_end() const { return iter.at_end(); }

    inline matcher_tuple_args& next()
    {
        iter.next();
        return *this;
    }

    inline matcher_tuple_args& push_mapping()
    {
        if (mapping) mapping->push_back(iter.position());
        return *this;
    }

    inline const uniform_type_info* type() const
    {
        return &(iter.type());
    }

    inline const void* value() const
    {
        return iter.value_ptr();
    }

};

bool do_match(matcher_types_args& ty_args, matcher_tuple_args& tu_args)
{
    // nullptr == wildcard
    if (ty_args.at_end() && tu_args.at_end())
    {
        return true;
    }
    if (ty_args.type() == nullptr)
    {
        // perform submatching
        ty_args.next();
        if (ty_args.at_end())
        {
            // always true at the end of the pattern
            return true;
        }
        auto ty_cpy = ty_args;
        std::vector<size_t> mv;
        auto mv_ptr = (tu_args.mapping) ? &mv : nullptr;
        // iterate over tu_iter until we found a match
        while (tu_args.at_end() == false)
        {
            matcher_tuple_args tu_cpy(tu_args.iter, mv_ptr);
            if (do_match(ty_cpy, tu_cpy))
            {
                if (mv_ptr)
                {
                    tu_args.mapping->insert(tu_args.mapping->end(),
                                            mv.begin(),
                                            mv.end());
                }
                return true;
            }
            // next iteration
            mv.clear();
            tu_args.next();
        }
    }
    else
    {
        if (tu_args.at_end() == false && ty_args.type() == tu_args.type())
        {
            if (   ty_args.has_value() == false
                || ty_args.type()->equal(ty_args.value(), tu_args.value()))
            {
                tu_args.push_mapping();
                return do_match(ty_args.next(), tu_args.next());
            }
        }
    }
    return false;
}

template<typename... Types>
struct foobar_test
{

    typedef typename cppa::util::filter_type_list<wildcard, cppa::util::type_list<Types...> >::type filtered_types;

    typename tdata_from_type_list<filtered_types>::type m_data;

    const void* m_data_ptr[sizeof...(Types)];
    const cppa::uniform_type_info* m_utis[sizeof...(Types)];

    foobar_test()
    {
        const cppa::uniform_type_info** iter = m_utis;
        fill_vec<decltype(iter), Types...>(iter);
        for (size_t i = 0; i < sizeof...(Types); ++i)
        {
            m_data_ptr[i] = nullptr;
        }
    }

    template<typename Arg0, typename... Args>
    foobar_test(const Arg0& arg0, const Args&... args) : m_data(arg0, args...)
    {
        const cppa::uniform_type_info** iter = m_utis;
        fill_vec<decltype(iter), Types...>(iter);
        size_t j = 0;
        for (size_t i = 0; i < sizeof...(Types); ++i)
        {
            if (m_utis[i] == nullptr)
            {
                m_data_ptr[i] = nullptr;
            }
            else
            {
                if (j < (sizeof...(Args) + 1))
                {
                    m_data_ptr[i] = m_data.at(j++);
                }
                else
                {
                    m_data_ptr[i] = nullptr;
                }
            }
        }
    }

    const cppa::uniform_type_info& operator[](size_t pos) const
    {
        return *(m_utis[pos]);
    }

    bool operator()(const cppa::any_tuple& msg) const
    {
        matcher_types_args ty(sizeof...(Types), m_data_ptr, m_utis);
        matcher_tuple_args tu(msg);
        return do_match(ty, tu);
    }

};

} // namespace <anonmyous> w/ using cppa

int main(int argc, char** argv)
{

    cout << std::boolalpha;

    auto x = cppa::make_tuple(0, 0.0f, 0.0);

    foobar_test<wildcard> ft0;

    foobar_test<int,float,double> ft1;
/*
    cout << "ft1[0] = " << ft1[0].name() << ", "
         << "ft1[1] = " << ft1[1].name() << ", "
         << "ft1[2] = " << ft1[2].name()
         << endl;
*/
    cout << "ft0(x) [expected: true] = " << ft0(x) << endl;
    cout << "ft1(x) [expected: true] = " << ft1(x) << endl;

    foobar_test<int,float,double> ft2(0);
    cout << "ft2(x) [expected: true] = " << std::boolalpha << ft2(x) << endl;

    foobar_test<int,float,double> ft3(0, 0.f);
    cout << "ft3(x) [expected: true] = " << std::boolalpha << ft3(x) << endl;

    foobar_test<int,float,double> ft4(0, 0.f, 0.0);
    cout << "ft4(x) [expected: true] = " << std::boolalpha << ft4(x) << endl;

    foobar_test<int,float,double> ft5(0, 0.1f, 0.0);
    cout << "ft5(x) [expected: false] = " << std::boolalpha << ft5(x) << endl;

    return 0;

    auto args = get_kv_pairs(argc, argv);
    if (!args.empty())
    {
        decltype(args.find("")) i;
        if (found_key(i, args, "run"))
        {
            auto& what = i->second;
/*            if (what == "performance_test")
            {
                cout << endl << "run queue performance test ... " << endl;
                test__queue_performance();
                return 0;
            }
            else*/ if (what == "remote_actor")
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
    //cout << endl << endl;
    std::cout << std::boolalpha;
    size_t errors = 0;
    RUN_TEST(test__yield_interface);
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
    RUN_TEST_A3(test__remote_actor, argv[0], false, args);
    cout << endl
         << "error(s) in all tests: " << errors
         << endl;
    return 0;
}
