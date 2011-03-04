#ifndef CPPA_UTIL_REVERSE_TYPE_LIST_HPP
#define CPPA_UTIL_REVERSE_TYPE_LIST_HPP

#include "cppa/util/type_list.hpp"
#include "cppa/util/concat_type_lists.hpp"

namespace cppa { namespace util {

template<typename ListFrom, typename ListTo = type_list<>>
struct reverse_type_list;

template<typename HeadA, typename... TailA, typename... ListB>
struct reverse_type_list<type_list<HeadA, TailA...>, type_list<ListB...>>
{
	typedef typename reverse_type_list<type_list<TailA...>,
									   type_list<HeadA, ListB...>>::type
			type;
};

template<typename... Types>
struct reverse_type_list<type_list<>, type_list<Types...>>
{
	typedef type_list<Types...> type;
};

} } // namespace cppa::util

#endif // CPPA_UTIL_REVERSE_TYPE_LIST_HPP
