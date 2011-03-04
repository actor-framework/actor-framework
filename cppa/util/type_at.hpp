#ifndef CPPA_UTIL_TYPE_AT_HPP
#define CPPA_UTIL_TYPE_AT_HPP

// std::size_t
#include <cstddef>

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

template<std::size_t N, typename TypeList>
struct type_at
{
	typedef typename type_at<N-1, typename TypeList::tail_type>::type type;
};

template<typename TypeList>
struct type_at<0, TypeList>
{
	typedef typename TypeList::head_type type;
};

} } // namespace cppa::util

#endif // CPPA_UTIL_TYPE_AT_HPP
