#ifndef EVAL_FIRST_N_HPP
#define EVAL_FIRST_N_HPP

#include <type_traits>

#include "cppa/util/first_n.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/eval_type_lists.hpp"

namespace cppa { namespace util {

template <std::size_t N,
          class ListA, class ListB,
          template <typename, typename> class What>
struct eval_first_n;

template <std::size_t N, typename... TypesA, typename... TypesB,
          template <typename, typename> class What>
struct eval_first_n<N, type_list<TypesA...>, type_list<TypesB...>, What>
{
    typedef type_list<TypesA...> first_list;
    typedef type_list<TypesB...> second_list;

    typedef typename first_n<N, first_list>::type slist_a;
    typedef typename first_n<N, second_list>::type slist_b;

    static const bool value = eval_type_lists<slist_a, slist_b, What>::value;
};

} } // namespace cppa::util

#endif // EVAL_FIRST_N_HPP
