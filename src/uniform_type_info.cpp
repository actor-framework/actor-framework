#include <limits>
#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <stdexcept>

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

template<> struct int_helper<1,true> { static const char* name; };
const char* int_helper<1,true>::name = "int8_t";

template<> struct int_helper<2,true> { static const char* name; };
const char* int_helper<2,true>::name = "int16_t";

template<> struct int_helper<4,true> { static const char* name; };
const char* int_helper<4,true>::name = "int32_t";

template<> struct int_helper<8,true> { static const char* name; };
const char* int_helper<8,true>::name = "int64_t";

template<> struct int_helper<1,false> { static const char* name; };
const char* int_helper<1,false>::name = "uint8_t";

template<> struct int_helper<2,false> { static const char* name; };
const char* int_helper<2,false>::name = "uint16_t";

template<> struct int_helper<4,false> { static const char* name; };
const char* int_helper<4,false>::name = "uint32_t";

template<> struct int_helper<8,false> { static const char* name; };
const char* int_helper<8,false>::name = "uint64_t";

template<typename T>
struct uniform_int : int_helper<sizeof(T), std::numeric_limits<T>::is_signed>{};

std::map<std::string, cppa::utype*>* ut_map = 0;

#ifdef CPPA_GCC
template<typename T>
std::string raw_name()
{
	std::string result;
	size_t size;
	int status;
	char* undecorated = abi::__cxa_demangle(typeid(T).name(),
											NULL, &size, &status);
	assert(status == 0);
	result = undecorated;
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

string_map* btm_ptr = 0;

const string_map& builtin_type_mappings()
{
	if (!btm_ptr)
	{
		btm_ptr = new string_map
		{
			{ raw_name<char>(),
			  uniform_int<char>::name               },
			{ raw_name<signed char>(),
			  uniform_int<signed char>::name        },
			{ raw_name<unsigned char>(),
			  uniform_int<unsigned char>::name      },
			{ raw_name<short>(),
			  uniform_int<short>::name              },
			{ raw_name<signed short>(),
			  uniform_int<signed short>::name       },
			{ raw_name<unsigned short>(),
			  uniform_int<unsigned short>::name     },
			{ raw_name<short int>(),
			  uniform_int<short int>::name          },
			{ raw_name<signed short int>(),
			  uniform_int<signed short int>::name   },
			{ raw_name<unsigned short int>(),
			  uniform_int<unsigned short int>::name },
			{ raw_name<int>(),
			  uniform_int<int>::name                },
			{ raw_name<signed int>(),
			  uniform_int<signed int>::name         },
			{ raw_name<unsigned int>(),
			  uniform_int<unsigned int>::name       },
			{ raw_name<long>(),
			  uniform_int<long>::name               },
			{ raw_name<signed long>(),
			  uniform_int<signed long>::name        },
			{ raw_name<unsigned long>(),
			  uniform_int<unsigned long>::name      },
			{ raw_name<long int>(),
			  uniform_int<long int>::name           },
			{ raw_name<signed long int>(),
			  uniform_int<signed long int>::name    },
			{ raw_name<unsigned long int>(),
			  uniform_int<unsigned long int>::name  },
			{ raw_name<long long>(),
			  uniform_int<long long>::name          },
			{ raw_name<signed long long>(),
			  uniform_int<signed long long>::name   },
			{ raw_name<unsigned long long>(),
			  uniform_int<unsigned long long>::name },
	  // GCC dosn't return a standard compliant name for the std::string typedef
#	  ifdef CPPA_GCC
	  { raw_name<std::string>(),
		"std::basic_string<char,std::char_traits<char>,std::allocator<char>>" }
#	  endif
		};
	}
	return *btm_ptr;
}

} // namespace <anonymous>

namespace cppa { namespace detail {

std::string demangle(const char* type_name)
{
	std::string result;
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
	size_t size;
	int status;
	char* undecorated = abi::__cxa_demangle(type_name,
											NULL, &size, &status);
	assert(status == 0);
	result = undecorated;
	free(undecorated);
#	else
#		error "Unsupported plattform"
#	endif
	// template class?
	std::string::size_type pos = result.find('<');
	if (pos != std::string::npos)
	{
		// map every single template argument to uniform names
		// skip leading '<'
		++pos;
		std::string processed_name(result, 0, pos);
		processed_name.reserve(result.size());
		std::string tmp;
		// replace all simple type names
		for (std::string::size_type p = pos; p < result.size(); ++p)
		{
			switch (result[p])
			{

			 case ',':
			 case '>':
			 case '<':
				{
					// erase trailing whitespaces
					while (!tmp.empty() && tmp[tmp.size() - 1] == ' ')
					{
						tmp.erase(tmp.size() - 1);
					}
					auto i = builtin_type_mappings().find(tmp);
					if (i != builtin_type_mappings().end())
					{
						processed_name += i->second;
					}
					else
					{
						processed_name += tmp;
					}
				}
				processed_name += result[p];
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
				tmp += result[p];
				break;

			}
		}
		/*
		if (!m_complex_mappings.empty())
		{
			// perform a lookup for complex types, such as template types
			for (string_map::const_iterator i = m_complex_mappings.begin();
				 i != m_complex_mappings.end();
				 ++i)
			{
				const std::string& needle = i->first;
				std::string::size_type x = processed_name.find(needle);
				if (x != std::string::npos)
				{
					processed_name.replace(x, needle.size(), i->second);
				}
			}
		}
		*/
		result = processed_name;
	}
	else
	{
		auto i = builtin_type_mappings().find(result);
		if (i != builtin_type_mappings().end())
		{
			result = i->second;
		}
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
