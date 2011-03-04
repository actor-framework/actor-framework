#ifndef CPPA_UTIL_TYPE_LIST_POP_BACK_HPP
#define CPPA_UTIL_TYPE_LIST_POP_BACK_HPP

#include "cppa/util/type_list.hpp"
#include "cppa/util/reverse_type_list.hpp"

namespace cppa { namespace util {

template<typename List>
struct type_list_pop_back
{
	typedef typename reverse_type_list<List>::type rlist;
	typedef typename reverse_type_list<typename rlist::tail_type>::type type;
};

} } // namespace cppa::util

#endif // CPPA_UTIL_TYPE_LIST_POP_BACK_HPP
