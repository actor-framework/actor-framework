#ifndef FILTER_TYPE_LIST_HPP
#define FILTER_TYPE_LIST_HPP

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

template<typename Needle, typename Haystack>
struct filter_type_list;

template<typename Needle, typename... Tail>
struct filter_type_list<Needle, util::type_list<Needle, Tail...>>
{
	typedef typename filter_type_list<Needle, util::type_list<Tail...>>::type type;
};

template<typename Needle, typename... Tail>
struct filter_type_list<Needle, util::type_list<Needle*, Tail...>>
{
	typedef typename filter_type_list<Needle, util::type_list<Tail...>>::type type;
};

template<typename Needle, typename Head, typename... Tail>
struct filter_type_list<Needle, util::type_list<Head, Tail...>>
{
	typedef typename concat_type_lists<util::type_list<Head>, typename filter_type_list<Needle, util::type_list<Tail...>>::type>::type type;
};

template<typename Needle>
struct filter_type_list<Needle, util::type_list<>>
{
	typedef util::type_list<> type;
};

} } // namespace cppa::util

#endif // FILTER_TYPE_LIST_HPP
