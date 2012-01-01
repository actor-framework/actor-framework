#ifndef FROM_STRING_HPP
#define FROM_STRING_HPP

#include <string>
#include <typeinfo>
#include <exception>

#include "cppa/object.hpp"
#include "cppa/uniform_type_info.hpp"

namespace cppa {

object from_string(std::string const& what);

template<typename T>
T from_string(const std::string &what)
{
    object o = from_string(what);
    std::type_info const& tinfo = typeid(T);
    if (o.type() == tinfo)
    {
        return std::move(get<T>(o));
    }
    else
    {
        std::string error_msg = "expected type name ";
        error_msg += uniform_typeid(tinfo)->name();
        error_msg += " found ";
        error_msg += o.type().name();
        throw std::logic_error(error_msg);
    }
}

} // namespace cppa

#endif // FROM_STRING_HPP
