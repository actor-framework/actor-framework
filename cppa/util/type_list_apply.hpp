#ifndef TYPE_LIST_APPLY_HPP
#define TYPE_LIST_APPLY_HPP

#include "cppa/util/type_list.hpp"
#include "cppa/util/concat_type_lists.hpp"

namespace cppa { namespace util {

/**
 * @brief Apply @p What to each element of @p List.
 */
template <typename List, template <typename> class What>
struct type_list_apply
{
    typedef typename What<typename List::head_type>::type head_type;
    typedef typename concat_type_lists<
                            type_list<head_type>,
                            typename type_list_apply<typename List::tail_type, What>::type>::type
            type;
};

template<template <typename> class What>
struct type_list_apply<type_list<>, What>
{
    typedef type_list<> type;
};

} } // namespace cppa::util

#endif // TYPE_LIST_APPLY_HPP
