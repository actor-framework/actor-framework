#ifndef TO_UNIFORM_NAME_HPP
#define TO_UNIFORM_NAME_HPP

#include <string>
#include <typeinfo>

namespace cppa { namespace detail {

std::string to_uniform_name(const std::string& demangled_name);
std::string to_uniform_name(const std::type_info& tinfo);

} } // namespace cppa::detail

#endif // TO_UNIFORM_NAME_HPP
