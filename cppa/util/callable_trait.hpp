#ifndef CPPA_UTIL_CALLABLE_TRAIT
#define CPPA_UTIL_CALLABLE_TRAIT

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

template<typename Signature>
struct callable_trait;

template<class C, typename Result, typename... Args>
struct callable_trait<Result (C::*)(Args...) const>
{
	typedef Result result_type;
	typedef util::type_list<Args...> arg_types;
};

template<class C, typename Result, typename... Args>
struct callable_trait<Result (C::*)(Args...)>
{
	typedef Result result_type;
	typedef util::type_list<Args...> arg_types;
};

template<typename Result, typename... Args>
struct callable_trait<Result (Args...)>
{
	typedef Result result_type;
	typedef util::type_list<Args...> arg_types;
};

template<typename Result, typename... Args>
struct callable_trait<Result (*)(Args...)>
{
	typedef Result result_type;
	typedef util::type_list<Args...> arg_types;
};

} } // namespace cppa::util

#endif // CPPA_UTIL_CALLABLE_TRAIT
