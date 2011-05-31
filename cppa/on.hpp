#ifndef ON_HPP
#define ON_HPP

#include "cppa/match.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/invoke_rules.hpp"

#include "cppa/util/eval_first_n.hpp"
#include "cppa/util/filter_type_list.hpp"

#include "cppa/detail/ref_counted_impl.hpp"

namespace cppa { namespace detail {

// irb_helper is not thread safe
struct irb_helper : ref_counted_impl<size_t>
{
    virtual ~irb_helper() { }
    virtual bool value_cmp(const any_tuple&, std::vector<size_t>&) const = 0;
};

template<typename... Types>
struct invoke_rule_builder
{

 private:

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

        static constexpr size_t num_args = sizeof...(Args) + 1;

        static_assert(num_args <= filtered_types::size,
                      "too much arguments");

        class helper_impl : public irb_helper
        {

            tuple<Arg0, Args...> m_values;

         public:

            helper_impl(const Arg0& arg0, const Args&... args)
                : m_values(arg0, args...)
            {
            }

            virtual bool value_cmp(const any_tuple& t,
                                   std::vector<size_t>& v) const
            {
                return match<Types...>(t, m_values, v);
            }

        };

        m_helper = new helper_impl(arg0, args...);

        static_assert(util::eval_first_n<num_args,
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
        if (!m_helper) // don't match on values
        {
            auto inv = [f](const any_tuple& t) -> bool
            {
                std::vector<size_t> mappings;
                if (match<Types...>(t, mappings))
                {
                    tuple_view_type tv(t.vals(), std::move(mappings));
                    invoke(f, tv);
                    return true;
                }
                return false;
            };
            auto gt = [sub_inv](const any_tuple& t) -> intermediate*
            {
                std::vector<size_t> mappings;
                if (match<Types...>(t, mappings))
                {
                    tuple_view_type tv(t.vals(), std::move(mappings));
                    typedef intermediate_impl<decltype(sub_inv),tuple_view_type>
                            timpl;
                    return new timpl(sub_inv, tv);
                }
                return 0;
            };
            typedef invokable_impl<decltype(inv), decltype(gt)> iimpl;
            return invoke_rules(new iimpl(std::move(inv), std::move(gt)));
        }
        else // m_helper matches on values
        {
            auto inv = [f, m_helper](const any_tuple& t) -> bool
            {
                std::vector<size_t> mappings;
                if (m_helper->value_cmp(t, mappings))
                {
                    tuple_view_type tv(t.vals(), std::move(mappings));
                    invoke(f, tv);
                    return true;
                }
                return false;
            };
            auto gt = [sub_inv, m_helper](const any_tuple& t) -> intermediate*
            {
                std::vector<size_t> mappings;
                if (m_helper->value_cmp(t, mappings))
                {
                    tuple_view_type tv(t.vals(), std::move(mappings));
                    typedef intermediate_impl<decltype(sub_inv),tuple_view_type>
                            timpl;
                    return new timpl(sub_inv, tv);
                }
                return 0;
            };
            typedef invokable_impl<decltype(inv), decltype(gt)> iimpl;
            return invoke_rules(new iimpl(std::move(inv), std::move(gt)));
        }
    }

};

template<>
struct invoke_rule_builder<any_type*>
{

    template<typename F>
    invoke_rules operator>>(F f)
    {
        auto inv = [f](const any_tuple&) -> bool
        {
            f();
            return true;
        };
        auto gt = [f](const any_tuple&) -> intermediate*
        {
            return new intermediate_impl<decltype(f)>(f);
        };
        typedef invokable_impl<decltype(inv), decltype(gt)> iimpl;
        return invoke_rules(new iimpl(std::move(inv), std::move(gt)));
    }

};

} } // cppa::detail

namespace cppa {

template<typename... Types>
inline detail::invoke_rule_builder<Types...> on()
{
    return detail::invoke_rule_builder<Types...>();
}

inline detail::invoke_rule_builder<any_type*> others()
{
    return on<any_type*>();
}

template<typename... Types, typename Arg0, typename... Args>
inline detail::invoke_rule_builder<Types...> on(const Arg0& arg0,
                                                const Args&... args)
{
    return detail::invoke_rule_builder<Types...>(arg0, args...);
}

} // namespace cppa

#endif // ON_HPP
