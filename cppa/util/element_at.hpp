#ifndef ELEMENT_AT_HPP
#define ELEMENT_AT_HPP

#include "cppa/util/at.hpp"

namespace cppa { namespace util {

template<size_t N, class C>
struct element_at;

template<size_t N, template<typename...> class C, typename... Tn>
struct element_at<N, C<Tn...>> : at<N, Tn...>
{
};

} } // namespace cppa::util

#endif // ELEMENT_AT_HPP
