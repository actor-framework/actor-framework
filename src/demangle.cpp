#include <stdexcept>

#include "cppa/config.hpp"
#include "cppa/detail/demangle.hpp"

#ifdef CPPA_GCC
#include <cxxabi.h>
#endif

namespace cppa { namespace detail {

std::string demangle(const char* typeid_name)
{
	size_t size;
	int status;
	char* undecorated = abi::__cxa_demangle(typeid_name, NULL, &size, &status);
	if (status != 0)
	{
		std::string error_msg = "Could not demangle type name ";
		error_msg += typeid_name;
		throw std::logic_error(error_msg);
	}
	std::string result; // the undecorated typeid name
	result.reserve(size);
	const char* cstr = undecorated;
	// this loop filter unnecessary characters from undecorated
	for (char c = *cstr; c != '\0'; c = *++cstr)
	{
		if (c == ' ')
		{
			char previous_c = result.empty() ? ' ' : *(result.rbegin());
			// get next non-space character
			do { c = *++cstr; } while (c == ' ');
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
		}
		else
		{
			result += c;
		}
	}
	free(undecorated);
	return result;
}

} } // namespace cppa::detail
