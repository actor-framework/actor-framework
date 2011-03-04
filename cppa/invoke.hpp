#ifndef INVOKE_HPP
#define INVOKE_HPP

#include <type_traits>

#include "cppa/tuple.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/untyped_tuple.hpp"

#include "cppa/util/type_at.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/callable_trait.hpp"
#include "cppa/util/reverse_type_list.hpp"
#include "cppa/util/type_list_pop_back.hpp"

namespace cppa { namespace detail {

template<std::size_t N, typename F, typename Tuple,
		 typename ArgTypeList, typename... Args>
struct invoke_helper
{
	typedef typename util::reverse_type_list<ArgTypeList>::type::head_type back_type;
	typedef typename util::type_at<N, Tuple>::type tuple_val_type;
	typedef typename util::type_list_pop_back<ArgTypeList>::type next_list;
	inline static void _(F& f, const Tuple& t, const Args&... args)
	{
		static_assert(std::is_convertible<tuple_val_type, back_type>::value,
					  "tuple element is not convertible to expected argument");
		invoke_helper<N - 1, F, Tuple, next_list, tuple_val_type, Args...>
				::_(f, t, t.get<N>(), args...);
	}
};

template<std::size_t N, typename F, typename Tuple, typename... Args>
struct invoke_helper<N, F, Tuple, util::type_list<>, Args...>
{
	inline static void _(F& f, const Tuple&, const Args&... args)
	{
		f(args...);
	}
};

template<bool HasCallableTrait, typename Tuple, typename F>
struct invoke_impl
{

	typedef typename util::callable_trait<F>::arg_types arg_types;

	static const std::size_t tuple_size = Tuple::type_list_size - 1;

	inline static void _(F& f, const Tuple& t)
	{
		invoke_helper<tuple_size, F, Tuple, arg_types>::_(f, t);
	}

};

template<typename Tuple, typename F>
struct invoke_impl<false, Tuple, F>
{

	typedef typename util::callable_trait<decltype(&F::operator())>::arg_types
			arg_types;

	static const std::size_t tuple_size = Tuple::type_list_size - 1;

	inline static void _(F& f, const Tuple& t)
	{
		invoke_helper<tuple_size, F, Tuple, arg_types>::_(f, t);
	}

};

} } // namespace cppa::detail

namespace cppa {

template<typename Tuple, typename F>
void invoke(F what, const Tuple& args)
{
	typedef typename std::remove_pointer<F>::type f_type;
	detail::invoke_impl<std::is_function<f_type>::value, Tuple, F>::_(what, args);
}

} // namespace cppa

#endif // INVOKE_HPP
