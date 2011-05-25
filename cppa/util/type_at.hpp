#ifndef CPPA_UTIL_TYPE_AT_HPP
#define CPPA_UTIL_TYPE_AT_HPP

// size_t
#include <cstddef>

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

/*
template<size_t N, typename TypeList>
struct type_at
{
    typedef typename type_at<N-1, typename TypeList::tail_type>::type type;
};

template<typename TypeList>
struct type_at<0, TypeList>
{
    typedef typename TypeList::head_type type;
};
*/

template<size_t N, class TypeList>
struct type_at;

template<size_t N, template<typename...> class TypeList, typename T0, typename... Tn>
struct type_at< N, TypeList<T0,Tn...> >
{
    // use type_list to avoid instantiations of TypeList parameter
    typedef typename type_at< N-1, type_list<Tn...> >::type type;
};

template<template<typename...> class TypeList, typename T0, typename... Tn>
struct type_at< 0, TypeList<T0,Tn...> >
{
    typedef T0 type;
};


template<size_t N, template<typename...> class TypeList>
struct type_at< N, TypeList<> >
{
    typedef void_type type;
};


} } // namespace cppa::util

#endif // CPPA_UTIL_TYPE_AT_HPP
