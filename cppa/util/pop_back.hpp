#ifndef POP_BACK_HPP
#define POP_BACK_HPP

#include "cppa/util/type_list.hpp"
#include "cppa/util/reverse_type_list.hpp"

namespace cppa { namespace util {

template<class C>
struct pop_back;

template<typename... Tn>
struct pop_back< type_list<Tn...> >
{
    typedef typename reverse_type_list<type_list<Tn...>>::type rlist;
    typedef typename reverse_type_list<typename rlist::tail_type>::type type;
};

} } // namespace cppa::util

#endif // POP_BACK_HPP
