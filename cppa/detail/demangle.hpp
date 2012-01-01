#ifndef DEMANGLE_HPP
#define DEMANGLE_HPP

#include <string>

namespace cppa { namespace detail {

std::string demangle(char const* typeid_name);

} } // namespace cppa::detail

#endif // DEMANGLE_HPP
