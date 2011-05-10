#ifndef APPLY_HPP
#define APPLY_HPP

namespace cppa { namespace util {

template<class C, template <typename> class... Traits>
struct apply;

template<class C>
struct apply<C>
{
    typedef C type;
};

template<class C,
         template <typename> class Trait0,
         template <typename> class... Traits>
struct apply<C, Trait0, Traits...>
{
    typedef typename apply<typename Trait0<C>::type, Traits...>::type type;
};

} }

#endif // APPLY_HPP
