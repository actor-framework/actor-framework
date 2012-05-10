#ifndef IS_BUILTIN_HPP
#define IS_BUILTIN_HPP

#include <type_traits>
#include "cppa/anything.hpp"

namespace cppa { namespace util {

template<typename T>
struct is_builtin
{
    static constexpr bool value = std::is_arithmetic<T>::value;
};

template<>
struct is_builtin<anything>
{
    static constexpr bool value = true;
};

} } // namespace cppa::util

#endif // IS_BUILTIN_HPP
