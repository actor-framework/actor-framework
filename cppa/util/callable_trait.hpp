#ifndef CPPA_UTIL_CALLABLE_TRAIT
#define CPPA_UTIL_CALLABLE_TRAIT

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

template<typename Signature>
struct callable_trait;

template<class C, typename ResultType, typename... Args>
struct callable_trait<ResultType (C::*)(Args...) const>
{
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

template<class C, typename ResultType, typename... Args>
struct callable_trait<ResultType (C::*)(Args...)>
{
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

template<typename ResultType, typename... Args>
struct callable_trait<ResultType (Args...)>
{
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

template<typename ResultType, typename... Args>
struct callable_trait<ResultType (*)(Args...)>
{
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

} } // namespace cppa::util

#endif // CPPA_UTIL_CALLABLE_TRAIT
