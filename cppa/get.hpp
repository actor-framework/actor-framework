#ifndef GET_HPP
#define GET_HPP

#include <cstddef>

#include "cppa/util/at.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa {

// forward declaration of detail::tdata
namespace detail { template<typename...> class tdata; }

// forward declaration of tuple
template<typename...> class tuple;

// forward declaration of tuple_view
template<typename...> class tuple_view;

// forward declaration of get(detail::tdata<...> const&)
template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(detail::tdata<Tn...> const&);

// forward declarations of get(tuple<...> const&)
template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(tuple<Tn...> const&);

// forward declarations of get(tuple_view<...> const&)
template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(tuple_view<Tn...> const&);

// forward declarations of get_ref(detail::tdata<...>&)
template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type& get_ref(detail::tdata<Tn...>&);

// forward declarations of get_ref(tuple<...>&)
template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type& get_ref(tuple<Tn...>&);

// forward declarations of get_ref(tuple_view<...>&)
template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type& get_ref(tuple_view<Tn...>&);

} // namespace cppa

#endif // GET_HPP
