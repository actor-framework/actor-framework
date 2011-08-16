#ifndef FIRST_N_HPP
#define FIRST_N_HPP

#include <cstddef>
#include <type_traits>

#include "cppa/util/at.hpp"
#include "cppa/util/wrapped.hpp"
#include "cppa/util/if_else.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/element_at.hpp"
#include "cppa/util/concat_type_lists.hpp"

namespace cppa { namespace detail {

template<size_t N, typename List>
struct n_
{

    typedef typename util::element_at<N - 1, List>::type head_type;

    typedef typename util::if_else<std::is_same<util::void_type, head_type>,
                                   util::type_list<>,
                                   util::wrapped<util::type_list<head_type>>>::type
            head_list;

    typedef typename n_<N - 1, List>::type tail_list;

    typedef typename util::concat_type_lists<tail_list, head_list>::type type;
};

template<typename List>
struct n_<0, List>
{
    typedef util::type_list<> type;
};

} } // namespace cppa::detail

namespace cppa { namespace util {

template<size_t N, typename List>
struct first_n;

template<size_t N, typename... ListTypes>
struct first_n<N, util::type_list<ListTypes...>>
{
    typedef typename detail::n_<N, util::type_list<ListTypes...>>::type type;
};

} } // namespace cppa::util

#endif // FIRST_N_HPP
