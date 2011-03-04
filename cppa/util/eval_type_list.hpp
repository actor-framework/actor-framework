#ifndef EVAL_TYPE_LIST_HPP
#define EVAL_TYPE_LIST_HPP

#include "cppa/util/type_list.hpp"
#include "cppa/util/concat_type_lists.hpp"

namespace cppa { namespace util {

/**
 * @brief Apply @p What to each element of @p List.
 */
template <class List, template <typename> class What>
struct eval_type_list
{
	typedef typename List::head_type head_type;
	typedef typename List::tail_type tail_type;

	static const bool value =
			   What<head_type>::value
			&& eval_type_list<tail_type, What>::value;
};

template <template <typename> class What>
struct eval_type_list<type_list<>, What>
{
	static const bool value = true;
};

} } // namespace cppa::util

#endif // EVAL_TYPE_LIST_HPP
