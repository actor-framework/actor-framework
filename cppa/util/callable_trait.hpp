#ifndef CPPA_UTIL_CALLABLE_TRAIT
#define CPPA_UTIL_CALLABLE_TRAIT

#include <type_traits>

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

template<bool IsFun, bool IsMemberFun, typename C>
struct get_callable_trait_helper
{
    typedef callable_trait<C> type;
};

template<typename C>
struct get_callable_trait_helper<false, false, C>
{
    typedef callable_trait<decltype(&C::operator())> type;
};

template<typename C>
struct get_callable_trait
{
    typedef typename
            get_callable_trait_helper<
                (   std::is_function<C>::value
                 || std::is_function<typename std::remove_pointer<C>::type>::value),
                std::is_member_function_pointer<C>::value,
                C>::type
            type;
};

} } // namespace cppa::util

#endif // CPPA_UTIL_CALLABLE_TRAIT
