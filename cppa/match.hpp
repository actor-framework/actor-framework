#ifndef MATCH_HPP
#define MATCH_HPP

#include <utility>
#include <stdexcept>

#include "cppa/any_type.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/untyped_tuple.hpp"

#include "cppa/util/compare_tuples.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/is_comparable.hpp"
#include "cppa/util/utype_iterator.hpp"
#include "cppa/util/eval_type_lists.hpp"
#include "cppa/util/filter_type_list.hpp"

#include "cppa/detail/matcher.hpp"

namespace cppa {

template<typename... MatchRules>
bool match(const untyped_tuple& what)
{
	util::utype_iterator begin = what.types().begin();
	util::utype_iterator end = what.types().end();
	return detail::matcher<MatchRules...>::match(begin, end);
}

template<typename... MatchRules>
bool match(const untyped_tuple& what, std::vector<std::size_t>& mappings)
{
	util::utype_iterator begin = what.types().begin();
	util::utype_iterator end = what.types().end();
	return detail::matcher<MatchRules...>::match(begin, end, &mappings);
}

template<typename... MatchRules, class ValuesTuple>
bool match(const untyped_tuple& what, const ValuesTuple& vals,
		   std::vector<std::size_t>& mappings)
{
	typedef util::type_list<MatchRules...> rules_list;
	typedef typename util::filter_type_list<any_type, rules_list>::type
			filtered_rules;
	typedef typename tuple_view_type_from_type_list<filtered_rules>::type
			view_type;
	static_assert(util::eval_type_lists<filtered_rules,
										ValuesTuple,
										util::is_comparable>::value,
				  "given values are not comparable to matched types");
	if (match<MatchRules...>(what, mappings))
	{
		std::vector<std::size_t> tmp(mappings);
		view_type view(what.vals(), std::move(tmp));
		return compare_first_elements(view, vals);
	}
	return false;
}

template<typename... MatchRules, class ValuesTuple>
bool match(const untyped_tuple& what, const ValuesTuple& vals)
{
	std::vector<std::size_t> mappings;
	return match<MatchRules...>(what, vals, mappings);
}

template<typename... MatchRules, template <typename...> class Tuple, typename... TupleTypes>
typename tuple_view_type_from_type_list<typename util::filter_type_list<any_type, util::type_list<MatchRules...>>::type>::type
get_view(const Tuple<TupleTypes...>& t)
{
	static_assert(util::a_matches_b<util::type_list<MatchRules...>,
									util::type_list<TupleTypes...>>::value,
				  "MatchRules does not match Tuple");
	std::vector<std::size_t> mappings;
	util::utype_iterator begin = t.types().begin();
	util::utype_iterator end = t.types().end();
	if (detail::matcher<MatchRules...>::match(begin, end, &mappings))
	{
		return { t.vals(), std::move(mappings) };
	}
	throw std::runtime_error("matcher did not return a valid mapping");
}

template<typename... MatchRules>
typename tuple_view_type_from_type_list<typename util::filter_type_list<any_type, util::type_list<MatchRules...>>::type>::type
get_view(const untyped_tuple& t)
{
	std::vector<std::size_t> mappings;
	if (match<MatchRules...>(t, mappings))
	{
		return { t.vals(), std::move(mappings) };
	}
	// todo: throw nicer exception
	throw std::runtime_error("doesn't match");
}

} // namespace cppa

#endif // MATCH_HPP
