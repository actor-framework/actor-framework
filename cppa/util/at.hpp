#ifndef AT_HPP
#define AT_HPP

namespace cppa { namespace util {

template<size_t N, typename... Tn>
struct at;

template<size_t N, typename T0, typename... Tn>
struct at<N, T0, Tn...>
{
    typedef typename at<N-1, Tn...>::type type;
};

template<typename T0, typename... Tn>
struct at<0, T0, Tn...>
{
    typedef T0 type;
};

} } // namespace cppa::util

#endif // AT_HPP
