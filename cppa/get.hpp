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

// forward declaration of get(const detail::tdata<...>&)
template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(const detail::tdata<Tn...>&);

// forward declarations of get(const tuple<...>&)
template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(const tuple<Tn...>&);

// forward declarations of get(const tuple_view<...>&)
template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(const tuple_view<Tn...>&);

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
