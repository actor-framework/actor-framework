#ifndef MATCHER_HPP
#define MATCHER_HPP

#include <vector>

#include "cppa/any_type.hpp"
#include "cppa/util/utype_iterator.hpp"

namespace cppa { namespace detail {

template<typename... Types> struct matcher;

template<typename Head, typename... Tail>
struct matcher<Head, Tail...>
{
	static bool match(util::utype_iterator& begin,
					  util::utype_iterator& end,
					  std::vector<std::size_t>* res = nullptr,
					  std::size_t pos = 0)
	{
		if (begin != end)
		{
			if (begin->native() == typeid(Head))
			{
				if (res)
				{
					res->push_back(pos);
				}
				++begin;
				++pos;
				return matcher<Tail...>::match(begin, end, res, pos);
			}
		}
		return false;
	}
};

template<typename... Tail>
struct matcher<any_type, Tail...>
{
	static bool match(util::utype_iterator &begin,
					  util::utype_iterator &end,
					  std::vector<std::size_t>* res = nullptr,
					  std::size_t pos = 0)
	{
		if (begin != end)
		{
			++begin;
			++pos;
			return matcher<Tail...>::match(begin, end, res, pos);
		}
		return false;
	}
};

template<typename Next, typename... Tail>
struct matcher<any_type*, Next, Tail...>
{
	static bool match(util::utype_iterator &begin,
					  util::utype_iterator &end,
					  std::vector<std::size_t>* res = nullptr,
					  std::size_t pos = 0)
	{
		bool result = false;
		while (!result)
		{
			util::utype_iterator begin_cpy = begin;
			result = matcher<Next, Tail...>::match(begin_cpy,end,nullptr,pos);
			if (!result)
			{
				++begin;
				++pos;
				if (begin == end) return false;
			}
			else if (res)
			{
				// one more iteration to store mappings
				begin_cpy = begin;
				(void) matcher<Next, Tail...>::match(begin_cpy, end, res, pos);
			}
		}
		return result;
	}
};

template<>
struct matcher<any_type*>
{
	static bool match(util::utype_iterator&,
					  util::utype_iterator&,
					  std::vector<std::size_t>* = nullptr,
					  std::size_t = 0)
	{
		return true;
	}
};

template<>
struct matcher<>
{
	static bool match(util::utype_iterator& begin,
					  util::utype_iterator& end,
					  std::vector<std::size_t>* = nullptr,
					  std::size_t = 0)
	{
		return begin == end;
	}
};

} } // namespace cppa::detail

#endif // MATCHER_HPP
