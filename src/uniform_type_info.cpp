#include <limits>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <stdexcept>

#include <atomic>

#include "cppa/config.hpp"
#include "cppa/uniform_type_info.hpp"

#ifdef CPPA_GCC
#include <cxxabi.h>
#endif

using std::cout;
using std::endl;

namespace {

typedef std::map<std::string, std::string> string_map;

template<int Size, bool IsSigned>
struct int_helper { };

template<>
struct int_helper<1, true>
{
	static std::string name() { return "@i8"; }
};

template<>
struct int_helper<2, true>
{
	static std::string name() { return "@i16"; }
};

template<>
struct int_helper<4, true>
{
	static std::string name() { return "@i32"; }
};

template<>
struct int_helper<8, true>
{
	static std::string name() { return "@i64"; }
};

template<>
struct int_helper<1, false>
{
	static std::string name() { return "@u8"; }
};

template<>
struct int_helper<2, false>
{
	static std::string name() { return "@u16"; }
};

template<>
struct int_helper<4, false>
{
	static std::string name() { return "@u32"; }
};

template<>
struct int_helper<8, false>
{
	static std::string name() { return "@u64"; }
};

template<typename T>
struct uniform_int : int_helper<sizeof(T), std::numeric_limits<T>::is_signed>{};

std::map<std::string, cppa::utype*>* ut_map = 0;

#ifdef CPPA_GCC
template<typename T>
std::string raw_name()
{
	size_t size;
	int status;
	char* undecorated = abi::__cxa_demangle(typeid(T).name(),
											NULL, &size, &status);
	assert(status == 0);
	std::string result(undecorated, size);
	free(undecorated);
	return result;
}
#elif defined(CPPA_WINDOWS)
template<typename T>
const char* raw_name()
{
	return typeid(T).name();
}
#endif

//string_map* btm_ptr = 0;

std::unique_ptr<string_map> btm_ptr;

const string_map& builtin_type_mappings()
{
	if (!btm_ptr)
	{
		string_map* value = new string_map
		{
			{ raw_name<char>(),
			  uniform_int<char>::name()               },
			{ raw_name<signed char>(),
			  uniform_int<signed char>::name()        },
			{ raw_name<unsigned char>(),
			  uniform_int<unsigned char>::name()      },
			{ raw_name<short>(),
			  uniform_int<short>::name()              },
			{ raw_name<signed short>(),
			  uniform_int<signed short>::name()       },
			{ raw_name<unsigned short>(),
			  uniform_int<unsigned short>::name()     },
			{ raw_name<short int>(),
			  uniform_int<short int>::name()          },
			{ raw_name<signed short int>(),
			  uniform_int<signed short int>::name()   },
			{ raw_name<unsigned short int>(),
			  uniform_int<unsigned short int>::name() },
			{ raw_name<int>(),
			  uniform_int<int>::name()                },
			{ raw_name<signed int>(),
			  uniform_int<signed int>::name()         },
			{ raw_name<unsigned int>(),
			  uniform_int<unsigned int>::name()       },
			{ raw_name<long>(),
			  uniform_int<long>::name()               },
			{ raw_name<signed long>(),
			  uniform_int<signed long>::name()        },
			{ raw_name<unsigned long>(),
			  uniform_int<unsigned long>::name()      },
			{ raw_name<long int>(),
			  uniform_int<long int>::name()           },
			{ raw_name<signed long int>(),
			  uniform_int<signed long int>::name()    },
			{ raw_name<unsigned long int>(),
			  uniform_int<unsigned long int>::name()  },
			{ raw_name<long long>(),
			  uniform_int<long long>::name()          },
			{ raw_name<signed long long>(),
			  uniform_int<signed long long>::name()   },
			{ raw_name<unsigned long long>(),
			  uniform_int<unsigned long long>::name() },
			{ raw_name<std::string>(),
			  "@str"                                  },
			{ raw_name<std::wstring>(),
			  "@wstr"                                 }
//		"std::basic_string<char,std::char_traits<char>,std::allocator<char>>" }
		};
		btm_ptr.reset(value);
		return *btm_ptr;
	}
	else
	{
		return *btm_ptr;
	}
}

} // namespace <anonymous>

namespace cppa { namespace detail {

std::string demangle_impl(const char* begin, const char* end, size_t size)
{
	// demangling of a template?
	const char* pos = std::find(begin, end, '<');
	if (pos == end)
	{
		return std::string(begin, size);
	}
	else
	{
		std::string processed(begin, pos);
		std::string tmp;
		// skip leading '<'
		for ( ++pos; pos != end; ++pos)
		{
			char c = *pos;
			switch (c)
			{

			case ',':
			case '>':
			case '<':
				{
					// erase trailing whitespaces
					while (!tmp.empty() && (*(tmp.rbegin()) == ' '))
					{
						tmp.resize(tmp.size() - 1);
					}
					auto i = builtin_type_mappings().find(tmp);
					if (i != builtin_type_mappings().end())
					{
						processed += i->second;
					}
					else
					{
						processed += tmp;
					}
				}
				processed += c;
				tmp.clear();
				break;

			case ' ':
				if (tmp == "class" || tmp == "struct")
				{
					tmp.clear();
				}
				else
				{
					// ignore leading spaces
					if (!tmp.empty()) tmp += ' ';
				}
				break;

			default:
				tmp += c;
				break;

			}
		}
		return processed;
	}
}

std::string demangle(const char* decorated_type_name)
{
#	ifdef CPPA_WINDOWS
	result = type_name;
	std::vector<std::string> needles;
	needles.push_back("class ");
	needles.push_back("struct ");
	// the VC++ compiler adds "class " and "struct " before a type names
	for (size_t i = 0; i < needles.size(); ++i)
	{
		const std::string& needle = needles[i];
		bool string_changed = false;
		do
		{
			// result is our haystack
			size_t pos = result.find(needle);
			if (pos != std::string::npos)
			{
				result.erase(pos, needle.size());
				string_changed = true;
			}
			else string_changed = false;
		}
		while (string_changed);
	}
#	elif defined(CPPA_GCC)
	size_t size = 0;
	int status = 0;
	char* undecorated = abi::__cxa_demangle(decorated_type_name,
											NULL, &size, &status);
	assert(status == 0);
	std::string result = demangle_impl(undecorated, undecorated + size, size);
	free(undecorated);
#	else
#		error "Unsupported plattform"
#	endif
	// template class?
	auto i = builtin_type_mappings().find(result);
	if (i != builtin_type_mappings().end())
	{
		result = i->second;
	}
	return result;
}

std::map<std::string, utype*>& uniform_types()
{
	if (!ut_map)
	{
		ut_map = new std::map<std::string, utype*>();
	}
	return *ut_map;
}

} } // namespace cppa

namespace cppa {

const utype& uniform_type_info(const std::string& uniform_type_name)
{
	decltype(detail::uniform_types()) ut = detail::uniform_types();
	auto i = ut.find(uniform_type_name);
	if (i == ut.end())
	{
		throw std::runtime_error("");
	}
	else
	{
		return *(i->second);
	}
}

} // namespace cppa
