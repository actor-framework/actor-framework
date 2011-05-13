#ifndef CONJUNCTION_HPP
#define CONJUNCTION_HPP

#include <type_traits>

namespace cppa { namespace util {

template<typename... BooleanConstants>
struct conjunction;

template<typename Head, typename... Tail>
struct conjunction<Head, Tail...>
    : std::integral_constant<bool, Head::value && conjunction<Tail...>::value>
{
};

template<>
struct conjunction<> : std::true_type { };

} } // namespace cppa::util

#endif // CONJUNCTION_HPP
