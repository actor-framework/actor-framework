#ifndef TO_STRING_HPP
#define TO_STRING_HPP

#include "cppa/uniform_type_info.hpp"

namespace cppa {

namespace detail {

std::string to_string_impl(void const* what, uniform_type_info const* utype);

} // namespace detail

/**
 * @brief Serializes a value to a string.
 * @param what A value of an announced type.
 * @returns A string representation of @p what.
 */
template<typename T>
std::string to_string(T const& what)
{
    auto utype = uniform_typeid<T>();
    if (utype == nullptr)
    {
        throw std::logic_error(  detail::to_uniform_name(typeid(T))
                               + " is not announced");
    }
    return detail::to_string_impl(&what, utype);
}

} // namespace cppa

#endif // TO_STRING_HPP
