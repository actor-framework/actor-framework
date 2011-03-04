#ifndef IS_ONE_OF_HPP
#define IS_ONE_OF_HPP

#include <type_traits>
#include "cppa/util/disjunction.hpp"

namespace cppa { namespace util {

template<typename T, typename... Types>
struct is_one_of;

template<typename T, typename X, typename... Types>
struct is_one_of<T, X, Types...>
	: disjunction<std::is_same<T, X>, is_one_of<T, Types...>>
{
};

template<typename T>
struct is_one_of<T> : std::false_type { };

} } // namespace cppa::util

#endif // IS_ONE_OF_HPP
