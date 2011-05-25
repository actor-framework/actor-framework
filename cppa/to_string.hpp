#ifndef TO_STRING_HPP
#define TO_STRING_HPP

#include "cppa/uniform_type_info.hpp"

namespace cppa {

namespace detail {

std::string to_string(const void* what, const uniform_type_info* utype);

} // namespace detail

template<typename T>
std::string to_string(const T& what)
{
    auto utype = uniform_typeid<T>();
    if (utype == nullptr)
    {
        throw std::logic_error(  detail::to_uniform_name(typeid(T))
                               + " is not announced");
    }
    return detail::to_string(&what, utype);
}

} // namespace cppa

#endif // TO_STRING_HPP
