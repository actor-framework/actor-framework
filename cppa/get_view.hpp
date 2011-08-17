#ifndef GET_VIEW_HPP
#define GET_VIEW_HPP

#include <vector>
#include <cstddef>

#include "cppa/match.hpp"
#include "cppa/tuple.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/util/a_matches_b.hpp"

namespace cppa {

/*
template<typename... MatchRules, template <typename...> class Tuple, typename... TupleTypes>
typename tuple_view_type_from_type_list<typename util::filter_type_list<any_type, util::type_list<MatchRules...>>::type>::type
get_view(const Tuple<TupleTypes...>& t)
{
    static_assert(util::a_matches_b<util::type_list<MatchRules...>,
                                    util::type_list<TupleTypes...>>::value,
                  "MatchRules does not match Tuple");
    std::vector<size_t> mappings;
    util::abstract_type_list::const_iterator begin = t.types().begin();
    util::abstract_type_list::const_iterator end = t.types().end();
    if (detail::matcher<MatchRules...>::match(begin, end, &mappings))
    {
        return { t.vals(), std::move(mappings) };
    }
    throw std::runtime_error("matcher did not return a valid mapping");
}
*/

template<typename... MatchRules>
typename tuple_view_type_from_type_list<typename util::filter_type_list<any_type, util::type_list<MatchRules...>>::type>::type
get_view(const any_tuple& ut)
{
    std::vector<size_t> mappings;
    if (match<MatchRules...>(ut, &mappings))
    {
        return { ut.vals(), std::move(mappings) };
    }
    // todo: throw nicer exception
    throw std::runtime_error("doesn't match");
}

} // namespace cppa

#endif // GET_VIEW_HPP
