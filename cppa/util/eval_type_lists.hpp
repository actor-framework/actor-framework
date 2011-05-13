#ifndef EVAL_TYPE_LISTS_HPP
#define EVAL_TYPE_LISTS_HPP

#include <type_traits>

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

/**
 * @brief Apply @p What to each element of @p List.
 */
template <class ListA, class ListB,
          template <typename, typename> class What>
struct eval_type_lists
{

    typedef typename ListA::head_type head_type_a;
    typedef typename ListA::tail_type tail_type_a;

    typedef typename ListB::head_type head_type_b;
    typedef typename ListB::tail_type tail_type_b;

    static const bool value =
               !std::is_same<head_type_a, void_type>::value
            && !std::is_same<head_type_b, void_type>::value
            && What<head_type_a, head_type_b>::value
            && eval_type_lists<tail_type_a, tail_type_b, What>::value;

};

template <template <typename, typename> class What>
struct eval_type_lists<type_list<>, type_list<>, What>
{
    static const bool value = true;
};

} } // namespace cppa::util

#endif // EVAL_TYPE_LISTS_HPP
