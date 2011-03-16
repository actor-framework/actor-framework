#ifndef ON_HPP
#define ON_HPP

#include "cppa/match.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/untyped_tuple.hpp"

#include "cppa/util/eval_first_n.hpp"
#include "cppa/util/filter_type_list.hpp"

#include "cppa/detail/ref_counted_impl.hpp"

namespace cppa { namespace detail {

struct irb_helper : ref_counted_impl<std::size_t>
{
	virtual ~irb_helper() { }
	virtual bool value_cmp(const untyped_tuple&,
						   std::vector<std::size_t>&) const = 0;
};

template<typename... Types>
struct invoke_rule_builder
{

	typedef util::type_list<Types...> types_list;

	typedef typename util::filter_type_list<any_type, types_list>::type
			filtered_types;

	intrusive_ptr<irb_helper> m_helper;

 public:

	invoke_rule_builder() { }

	template<typename Arg0, typename... Args>
	invoke_rule_builder(const Arg0& arg0, const Args&... args)
	{
		typedef util::type_list<Arg0, Args...> arg_types;

		static_assert(arg_types::type_list_size
					  <= filtered_types::type_list_size,
					  "too much arguments");

		class helper_impl : public irb_helper
		{

			tuple<Arg0, Args...> m_values;

		 public:

			helper_impl(const Arg0& arg0, const Args&... args)
				: m_values(arg0, args...)
			{
			}

			virtual bool value_cmp(const untyped_tuple& t,
								   std::vector<std::size_t>& v) const
			{
				return match<Types...>(t, m_values, v);
			}

		};

		m_helper = new helper_impl(arg0, args...);

		static_assert(util::eval_first_n<arg_types::type_list_size,
										 filtered_types,
										 arg_types,
										 util::is_comparable>::value,
					  "wrong argument types (not comparable)");
	}

	typedef typename tuple_view_type_from_type_list<filtered_types>::type
			tuple_view_type;

	template<typename F>
	invoke_rules operator>>(F f)
	{
		auto sub_inv = [f](const tuple_view_type& tv)
		{
			invoke(f, tv);
		};
		if (!m_helper)
		{
			auto inv = [f](const untyped_tuple& t) -> bool
			{
				std::vector<std::size_t> mappings;
				if (match<Types...>(t, mappings))
				{
					tuple_view_type tv(t.vals(), std::move(mappings));
					invoke(f, tv);
					return true;
				}
				return false;
			};
			auto gt = [sub_inv](const untyped_tuple& t) -> detail::intermediate*
			{
				std::vector<std::size_t> mappings;
				if (match<Types...>(t, mappings))
				{
					tuple_view_type tv(t.vals(), std::move(mappings));
					return new detail::intermediate_impl<decltype(sub_inv), tuple_view_type>(sub_inv, tv);
				}
				return 0;
			};
			return invoke_rules(new detail::invokable_impl<decltype(inv), decltype(gt)>(std::move(inv), std::move(gt)));
		}
		else
		{
			auto inv = [f, m_helper](const untyped_tuple& t) -> bool
			{
				std::vector<std::size_t> mappings;
				if (m_helper->value_cmp(t, mappings))
				{
					tuple_view_type tv(t.vals(), std::move(mappings));
					invoke(f, tv);
					return true;
				}
				return false;
			};
			auto gt = [sub_inv, m_helper](const untyped_tuple& t) -> detail::intermediate*
			{
				std::vector<std::size_t> mappings;
				if (m_helper->value_cmp(t, mappings))
				{
					tuple_view_type tv(t.vals(), std::move(mappings));
					return new detail::intermediate_impl<decltype(sub_inv), tuple_view_type>(sub_inv, tv);
				}
				return 0;
			};
			return invoke_rules(new detail::invokable_impl<decltype(inv), decltype(gt)>(std::move(inv), std::move(gt)));
		}
	}

};

} } // cppa::detail

namespace cppa {

template<typename... Types>
inline detail::invoke_rule_builder<Types...> on()
{
	return detail::invoke_rule_builder<Types...>();
}

template<typename... Types, typename Arg0, typename... Args>
inline detail::invoke_rule_builder<Types...> on(const Arg0& arg0, const Args&... args)
{
	return detail::invoke_rule_builder<Types...>(arg0, args...);
}

} // namespace cppa

#endif // ON_HPP
