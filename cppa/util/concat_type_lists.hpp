#ifndef CONCAT_TYPE_LISTS_HPP
#define CONCAT_TYPE_LISTS_HPP

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

template<typename ListA, typename ListB>
struct concat_type_lists;

template<typename... ListATypes, typename... ListBTypes>
struct concat_type_lists<util::type_list<ListATypes...>, util::type_list<ListBTypes...>>
{
	typedef util::type_list<ListATypes..., ListBTypes...> type;
};

} } // namespace cppa::util

#endif // CONCAT_TYPE_LISTS_HPP
