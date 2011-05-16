#ifndef GET_HPP
#define GET_HPP

#include <cstddef>

#include "cppa/util/type_at.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa {

// forward declaration of tuple
template<typename... ElementTypes>
class tuple;

// forward declaration of tuple_view
template<typename... ElementTypes>
class tuple_view;

// forward declarations of get(const tuple<...>&...)
template<size_t N, typename... Types>
const typename util::type_at<N, util::type_list<Types...>>::type&
get(const tuple<Types...>& t);

// forward declarations of get(const tuple_view<...>&...)
template<size_t N, typename... Types>
const typename util::type_at<N, util::type_list<Types...>>::type&
get(const tuple_view<Types...>& t);

// forward declarations of get_ref(tuple<...>&...)
template<size_t N, typename... Types>
typename util::type_at<N, util::type_list<Types...>>::type&
get_ref(tuple<Types...>& t);

// forward declarations of get_ref(tuple_view<...>&...)
template<size_t N, typename... Types>
typename util::type_at<N, util::type_list<Types...>>::type&
get_ref(tuple_view<Types...>& t);

} // namespace cppa

#endif // GET_HPP
