#ifndef MATCH_HPP
#define MATCH_HPP

#include <utility>
#include <stdexcept>

#include "cppa/any_type.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_view.hpp"

#include "cppa/util/compare_tuples.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/is_comparable.hpp"
#include "cppa/util/eval_type_lists.hpp"
#include "cppa/util/filter_type_list.hpp"

#include "cppa/detail/matcher.hpp"

namespace cppa {

template<typename... MatchRules>
bool match(const any_tuple& what)
{
    util::abstract_type_list::const_iterator begin = what.types().begin();
    util::abstract_type_list::const_iterator end = what.types().end();
    return detail::matcher<MatchRules...>::match(begin, end);
}

template<typename... MatchRules>
bool match(const any_tuple& what, std::vector<size_t>& mappings)
{
    util::abstract_type_list::const_iterator begin = what.types().begin();
    util::abstract_type_list::const_iterator end = what.types().end();
    return detail::matcher<MatchRules...>::match(begin, end, &mappings);
}

template<typename... MatchRules, class ValuesTuple>
bool match(const any_tuple& what, const ValuesTuple& vals,
           std::vector<size_t>& mappings)
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
        std::vector<size_t> tmp(mappings);
        view_type view(what.vals(), std::move(tmp));
        return util::compare_first_elements(view, vals);
    }
    return false;
}

template<typename... MatchRules, class ValuesTuple>
bool match(const any_tuple& what, const ValuesTuple& vals)
{
    std::vector<size_t> mappings;
    return match<MatchRules...>(what, vals, mappings);
}

} // namespace cppa

#endif // MATCH_HPP
