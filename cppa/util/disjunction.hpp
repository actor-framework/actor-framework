#ifndef DISJUNCTION_HPP
#define DISJUNCTION_HPP

#include <type_traits>

namespace cppa { namespace util {

template<typename... BooleanConstants>
struct disjunction;

template<typename Head, typename... Tail>
struct disjunction<Head, Tail...>
	: std::integral_constant<bool, Head::value || disjunction<Tail...>::value>
{
};

template<>
struct disjunction<> : std::false_type { };

} } // namespace cppa::util

#endif // DISJUNCTION_HPP
