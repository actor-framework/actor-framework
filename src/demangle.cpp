#include <stdexcept>

#include "cppa/config.hpp"
#include "cppa/detail/demangle.hpp"

#ifdef CPPA_GCC
#include <cxxabi.h>
#endif

namespace cppa { namespace detail {

std::string demangle(const char* decorated)
{
    size_t size;
    int status;
    char* undecorated = abi::__cxa_demangle(decorated, nullptr, &size, &status);
    if (status != 0)
    {
        std::string error_msg = "Could not demangle type name ";
        error_msg += decorated;
        throw std::logic_error(error_msg);
    }
    std::string result; // the undecorated typeid name
    result.reserve(size);
    const char* cstr = undecorated;
    // filter unnecessary characters from undecorated
    char c = *cstr;
    while (c != '\0')
    {
        if (c == ' ')
        {
            char previous_c = result.empty() ? ' ' : *(result.rbegin());
            // get next non-space character
            for (c = *++cstr; c == ' '; c = *++cstr) { }
            if (c != '\0')
            {
                // skip whitespace unless it separates two alphanumeric
                // characters (such as in "unsigned int")
                if (isalnum(c) && isalnum(previous_c))
                {
                    result += ' ';
                    result += c;
                }
                else
                {
                    result += c;
                }
                c = *++cstr;
            }
        }
        else
        {
            result += c;
            c = *++cstr;
        }
    }
    free(undecorated);
    return result;
}

} } // namespace cppa::detail
