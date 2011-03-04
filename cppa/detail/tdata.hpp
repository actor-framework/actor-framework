#ifndef TDATA_HPP
#define TDATA_HPP

#include "cppa/util/type_list.hpp"

#include "cppa/detail/abstract_tuple.hpp"

namespace cppa { namespace detail {

template<typename... ElementTypes>
struct tdata;

template<>
struct tdata<>
{

	typedef util::type_list<> element_types;

	typedef typename element_types::head_type head_type;

	typedef typename element_types::tail_type tail_type;

	static const std::size_t type_list_size = element_types::type_list_size;

	util::void_type head;

	tdata<>& tail()
	{
		throw std::out_of_range("");
	}

	const tdata<>& tail() const
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

	typedef util::type_list<Head, Tail...> element_types;

	typedef typename element_types::head_type head_type;

	typedef typename element_types::tail_type tail_type;

	static const std::size_t type_list_size = element_types::type_list_size;

	Head head;

	inline tdata() : super(), head() { }

	tdata(const Head& v0, const Tail&... vals) : super(vals...), head(v0) { }

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

};

template<std::size_t N, typename... ElementTypes>
struct tdata_upcast_helper;

template<std::size_t N, typename Head, typename... Tail>
struct tdata_upcast_helper<N, Head, Tail...>
{
	typedef typename tdata_upcast_helper<N-1, Tail...>::type type;
};

template<typename Head, typename... Tail>
struct tdata_upcast_helper<0, Head, Tail...>
{
	typedef tdata<Head, Tail...> type;
};

template<std::size_t N, typename... ElementTypes>
const typename util::type_at<N, util::type_list<ElementTypes...>>::type&
tdata_get(const tdata<ElementTypes...>& tv)
{
	static_assert(N < util::type_list<ElementTypes...>::type_list_size,
				  "N < tv.size()");
	return static_cast<const typename tdata_upcast_helper<N, ElementTypes...>::type&>(tv).head;
}

template<std::size_t N, typename... ElementTypes>
typename util::type_at<N, util::type_list<ElementTypes...>>::type&
tdata_get_ref(tdata<ElementTypes...>& tv)
{
	static_assert(N < util::type_list<ElementTypes...>::type_list_size,
				  "N >= tv.size()");
	return static_cast<typename tdata_upcast_helper<N, ElementTypes...>::type &>(tv).head;
}

} } // namespace cppa::detail

#endif // TDATA_HPP
