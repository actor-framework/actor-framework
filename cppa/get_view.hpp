#ifndef GET_VIEW_HPP
#define GET_VIEW_HPP

#include <vector>
#include <cstddef>

#include "cppa/tuple.hpp"
#include "cppa/pattern.hpp"
#include "cppa/anything.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/util/a_matches_b.hpp"

namespace cppa {

template<typename... MatchRules>
typename tuple_view_type_from_type_list<typename util::filter_type_list<anything, util::type_list<MatchRules...>>::type>::type
get_view(const any_tuple& ut)
{
    std::vector<size_t> mappings;
    pattern<MatchRules...> p;
    if (p(ut, &mappings))
    {
        return { ut.vals(), std::move(mappings) };
    }
    // todo: throw nicer exception
    throw std::runtime_error("doesn't match");
}

} // namespace cppa

#endif // GET_VIEW_HPP
