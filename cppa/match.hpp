#ifndef MATCH_HPP
#define MATCH_HPP

#include <utility>
#include <stdexcept>

#include "cppa/atom.hpp"
#include "cppa/any_type.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_view.hpp"

#include "cppa/util/compare_tuples.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/is_comparable.hpp"
#include "cppa/util/eval_type_lists.hpp"
#include "cppa/util/filter_type_list.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/unboxed.hpp"
#include "cppa/detail/matcher.hpp"

namespace cppa {

template<typename... TypeList, typename Arg0, typename... Args>
bool match(const any_tuple& what, std::vector<size_t>* mapping,
           const Arg0& arg0, const Args&... args)
{
    detail::matcher_arguments tmp(what, mapping);
    return detail::matcher<TypeList...>::match(tmp, arg0, args...);
}

template<typename... TypeList>
bool match(const any_tuple& what, std::vector<size_t>* mapping = nullptr)
{
    detail::matcher_arguments tmp(what, mapping);
    return detail::matcher<TypeList...>::match(tmp);
}

} // namespace cppa

#endif // MATCH_HPP
