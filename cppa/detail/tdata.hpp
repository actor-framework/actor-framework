#ifndef TDATA_HPP
#define TDATA_HPP

#include "cppa/get.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/filter_type_list.hpp"

#include "cppa/detail/abstract_tuple.hpp"

namespace cppa { namespace detail {

/*
 * just like std::tuple (but derives from ref_counted?)
 */
template<typename...>
struct tdata;

template<>
struct tdata<>
{

    util::void_type head;

    tdata<>& tail()
    {
        throw std::out_of_range("");
    }

    const tdata<>& tail() const
    {
        throw std::out_of_range("");
    }

    inline const void* at(size_t) const
    {
        throw std::out_of_range("");
    }

    inline bool operator==(const tdata&) const
    {
        return true;
    }

};

template<typename Head, typename... Tail>
struct tdata<Head, Tail...> : tdata<Tail...>
{

    typedef tdata<Tail...> super;

    Head head;

    inline tdata() : super(), head() { }

    //tdata(const Head& v0, const Tail&... vals) : super(vals...), head(v0) { }

    // allow partial initialization
    template<typename... Args>
    tdata(const Head& v0, const Args&... vals) : super(vals...), head(v0) { }

    inline tdata<Tail...>& tail()
    {
        // upcast
        return *this;
    }

    inline const tdata<Tail...>& tail() const
    {
        // upcast
        return *this;
    }

    inline bool operator==(const tdata& other) const
    {
        return head == other.head && tail() == other.tail();
    }

    inline const void* at(size_t pos) const
    {
        return (pos == 0) ? &head : super::at(pos - 1);
    }

};

template<size_t N, typename... Tn>
struct tdata_upcast_helper;

template<size_t N, typename Head, typename... Tail>
struct tdata_upcast_helper<N, Head, Tail...>
{
    typedef typename tdata_upcast_helper<N-1, Tail...>::type type;
};

template<typename Head, typename... Tail>
struct tdata_upcast_helper<0, Head, Tail...>
{
    typedef tdata<Head, Tail...> type;
};

template<typename T>
struct tdata_from_type_list;

template<typename... T>
struct tdata_from_type_list<cppa::util::type_list<T...>>
{
    typedef cppa::detail::tdata<T...> type;
};

} } // namespace cppa::detail

namespace cppa {

template<typename... Tn>
inline detail::tdata<Tn...> make_tdata(const Tn&... args)
{
    return detail::tdata<Tn...>(args...);
}

template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(const detail::tdata<Tn...>& tv)
{
    static_assert(N < sizeof...(Tn), "N >= tv.size()");
    return static_cast<const typename detail::tdata_upcast_helper<N, Tn...>::type&>(tv).head;
}

template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type& get_ref(detail::tdata<Tn...>& tv)
{
    static_assert(N < sizeof...(Tn), "N >= tv.size()");
    return static_cast<typename detail::tdata_upcast_helper<N, Tn...>::type &>(tv).head;
}

} // namespace cppa

#endif // TDATA_HPP
