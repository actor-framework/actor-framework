#ifndef MATCHER_HPP
#define MATCHER_HPP

#include <vector>

#include "cppa/any_type.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/is_one_of.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/abstract_type_list.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/unboxed.hpp"
#include "cppa/detail/matcher_arguments.hpp"

namespace cppa { namespace detail {

template<typename...>
struct matcher;

template<>
struct matcher<>
{
    static inline bool match(matcher_arguments& args)
    {
        return args.at_end();
    }
};

template<typename Head, typename... Tail>
struct matcher<Head, Tail...>
{

    typedef matcher<Tail...> next;

    static inline bool submatch(matcher_arguments& args)
    {
        return    args.at_end() == false
               && args.iter.type() == typeid(Head)
               && args.push_mapping();
    }

    static inline bool match(matcher_arguments& args)
    {
        return submatch(args) && next::match(args.next());
    }

    template<typename... Values>
    static inline bool match(matcher_arguments& args,
                             const Head& val0,
                             const Values&... values)
    {
        return    submatch(args)
               && args.iter.value<Head>() == val0
               && next::match(args.next(), values...);
    }

    template<typename Value0, typename... Values>
    static inline typename util::enable_if<util::is_one_of<Value0, any_type, util::wrapped<Head>>, bool>::type
    match(matcher_arguments& args, const Value0&, const Values&... values)
    {
        return submatch(args) && next::match(args.next(), values...);
    }

};

template<typename... Tail>
struct matcher<any_type, Tail...>
{
    typedef matcher<Tail...> next;
    static inline bool match(matcher_arguments& args)
    {
        return args.at_end() == false && next::match(args.next());
    }
    template<typename... Values>
    static inline bool match(matcher_arguments& args,
                             const any_type&,
                             const Values&... values)
    {
        return args.at_end() == false && next::match(args.next(), values...);
    }
};

template<>
struct matcher<any_type*>
{
    static inline bool match(matcher_arguments&, const any_type* = nullptr)
    {
        return true;
    }
};

template<typename Tail0, typename... Tail>
struct matcher<any_type*, Tail0, Tail...>
{

    template<typename... Values>
    static bool match_impl(matcher_arguments& args, const Values&... values)
    {
        std::vector<size_t> mv;
        auto mv_ptr = (args.mapping) ? &mv : nullptr;
        while (args.at_end() == false)
        {
            matcher_arguments cpy(args.iter, mv_ptr);
            if (matcher<Tail0, Tail...>::match(cpy, values...))
            {
                if (mv_ptr)
                {
                    args.mapping->insert(args.mapping->end(),
                                         mv.begin(), mv.end());
                }
                return true;
            }
            // next iteration
            mv.clear();
            args.next();
        }
        return false;
    }

    template<typename... Values>
    static inline bool match(matcher_arguments& args,
                      const any_type*,
                      const Values&... values)
    {
        return match_impl(args, values...);
    }

    static inline bool match(matcher_arguments& args)
    {
        return match_impl(args);
    }

};

} } // namespace cppa::detail

#endif // MATCHER_HPP
