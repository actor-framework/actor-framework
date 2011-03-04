#ifndef REPLACE_TYPE_HPP
#define REPLACE_TYPE_HPP

#include <type_traits>

#include "cppa/util/disjunction.hpp"

namespace cppa { namespace detail {

template<bool DoReplace, typename T, typename ReplaceType>
struct replace_type
{
	typedef T type;
};

template<typename T, typename ReplaceType>
struct replace_type<true, T, ReplaceType>
{
	typedef ReplaceType type;
};

} } // namespace cppa::detail

namespace cppa { namespace util {

template<typename What, typename With, typename... IfStmt>
struct replace_type
{
	static const bool do_replace = disjunction<IfStmt...>::value;
	typedef typename detail::replace_type<do_replace, What, With>::type
			type;
};

} } // namespace cppa::util

#endif // REPLACE_TYPE_HPP
