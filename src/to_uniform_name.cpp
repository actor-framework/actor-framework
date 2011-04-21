#include <map>
#include <cwchar>
#include <limits>
#include <vector>
#include <typeinfo>
#include <stdexcept>
#include <algorithm>

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace {

constexpr const char* mapped_int_names[][2] =
{
	{ nullptr,	nullptr	},	// sizeof 0 { invalid }
	{ "@i8",	"@u8"	},	// sizeof 1 -> signed / unsigned int8
	{ "@i16",	"@u16"	},	// sizeof 2 -> signed / unsigned int16
	{ nullptr,	nullptr	},	// sizeof 3 { invalid }
	{ "@i32",	"@u32"	},	// sizeof 4 -> signed / unsigned int32
	{ nullptr,	nullptr	},	// sizeof 5 { invalid }
	{ nullptr,	nullptr	},	// sizeof 6 { invalid }
	{ nullptr,	nullptr	},	// sizeof 7 { invalid }
	{ "@i64",	"@u64"	}	// sizeof 8 -> signed / unsigned int64
};

template<typename T>
constexpr size_t sign_index()
{
	static_assert(std::numeric_limits<T>::is_integer, "T is not an integer");
	return std::numeric_limits<T>::is_signed ? 0 : 1;
}

template<typename T>
inline std::string demangled()
{
	return cppa::detail::demangle(typeid(T).name());
}

template<typename T>
constexpr const char* mapped_int_name()
{
	return mapped_int_names[sizeof(T)][sign_index<T>()];
}

template<typename Iterator>
std::string to_uniform_name_impl(Iterator begin, Iterator end,
								 bool first_run = false)
{
	// all integer type names as uniform representation
	static std::map<std::string, std::string> mapped_demangled_names =
	{
	  { demangled<char>(), mapped_int_name<char>() },
	  { demangled<signed char>(), mapped_int_name<signed char>() },
	  { demangled<unsigned char>(), mapped_int_name<unsigned char>() },
	  { demangled<short>(), mapped_int_name<short>() },
	  { demangled<signed short>(), mapped_int_name<signed short>() },
	  { demangled<unsigned short>(), mapped_int_name<unsigned short>() },
	  { demangled<short int>(), mapped_int_name<short int>() },
	  { demangled<signed short int>(), mapped_int_name<signed short int>() },
	  { demangled<unsigned short int>(), mapped_int_name<unsigned short int>()},
	  { demangled<int>(), mapped_int_name<int>() },
	  { demangled<signed int>(), mapped_int_name<signed int>() },
	  { demangled<unsigned int>(), mapped_int_name<unsigned int>() },
	  { demangled<long int>(), mapped_int_name<long int>() },
	  { demangled<signed long int>(), mapped_int_name<signed long int>() },
	  { demangled<unsigned long int>(), mapped_int_name<unsigned long int>() },
	  { demangled<long>(), mapped_int_name<long>() },
	  { demangled<signed long>(), mapped_int_name<signed long>() },
	  { demangled<unsigned long>(), mapped_int_name<unsigned long>() },
	  { demangled<long long>(), mapped_int_name<long long>() },
	  { demangled<signed long long>(), mapped_int_name<signed long long>() },
	  { demangled<unsigned long long>(), mapped_int_name<unsigned long long>()},
	  { demangled<wchar_t>(), mapped_int_name<wchar_t>() },
	  { demangled<char16_t>(), mapped_int_name<char16_t>() },
	  { demangled<char32_t>(), mapped_int_name<char32_t>() },
	  { demangled<std::wstring>(), "@wstr" },
	  { demangled<std::string>(), "@str" }
	};

	// check if we could find the whole string in our lookup map
	if (first_run)
	{
		std::string tmp(begin, end);
		auto i = mapped_demangled_names.find(tmp);
		if (i != mapped_demangled_names.end())
		{
			return i->second;
		}
	}

	// does [begin, end) represents an empty string?
	if (begin == end) return "";
	// derived reverse_iterator type
	typedef std::reverse_iterator<Iterator> reverse_iterator;
	// a subsequence [begin', end') within [begin, end)
	typedef std::pair<Iterator, Iterator> subseq;
	std::vector<subseq> substrings;
	// explode string if we got a list of types
	int open_brackets = 0; // counts "open" '<'
	// denotes the begin of a possible subsequence
	Iterator anchor = begin;
	for (Iterator i = begin; i != end; /* i is incemented in the loop */)
	{
		switch (*i)
		{

		 case '<':
			++open_brackets;
			++i;
			break;

		 case '>':
			if (--open_brackets < 0)
			{
				throw std::runtime_error("malformed string");
			}
			++i;
			break;

		 case ',':
			if (open_brackets == 0)
			{
				substrings.push_back(std::make_pair(anchor, i));
				++i;
				anchor = i;
			}
			else
			{
				++i;
			}
			break;

		 default:
			++i;
			break;

		}
	}
	// call recursively for each list argument
	if (!substrings.empty())
	{
		std::string result;
		substrings.push_back(std::make_pair(anchor, end));
		for (const subseq& sstr : substrings)
		{
			if (!result.empty()) result += ",";
			result += to_uniform_name_impl(sstr.first, sstr.second);
		}
		return result;
	}
	// we didn't got a list, compute unify name
	else
	{
		// is [begin, end) a template?
		Iterator substr_begin = std::find(begin, end, '<');
		if (substr_begin == end)
		{
			// not a template, return mapping
			std::string arg(begin, end);
			auto mapped = mapped_demangled_names.find(arg);
			return (mapped == mapped_demangled_names.end()) ? arg : mapped->second;
		}
		// skip leading '<'
		++substr_begin;
		// find trailing '>'
		Iterator substr_end = std::find(reverse_iterator(end),
										reverse_iterator(substr_begin),
										'>')
							  // get as an Iterator
							  .base();
		// skip trailing '>'
		--substr_end;
		if (substr_end == substr_begin)
		{
			throw std::runtime_error("substr_end == substr_begin");
		}
		std::string result;
		// template name (part before leading '<')
		result.append(begin, substr_begin);
		// get mappings of all template parameter(s)
		result += to_uniform_name_impl(substr_begin, substr_end);
		result.append(substr_end, end);
		return result;
	}
}

} // namespace <anonymous>

namespace cppa { namespace detail {

std::string to_uniform_name(const std::string& dname)
{
	static std::string an = "(anonymous namespace)";
	static std::string an_replacement = "@_";
	auto r = to_uniform_name_impl(dname.begin(), dname.end(), true);
	// replace all occurrences of an with "@_"
	if (r.size() > an.size())
	{
		auto i = std::search(r.begin(), r.end(), an.begin(), an.end());
		while (i != r.end())
		{
			auto substr_end = i + an.size();
			r.replace(i, substr_end, an_replacement);
			// next iteration
			i = std::search(r.begin(), r.end(), an.begin(), an.end());
		}
	}
	return r;
}

} } // namespace cppa::detail
