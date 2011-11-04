#include "cppa/pattern.hpp"

namespace cppa { namespace detail {

bool do_match(pattern_arg& pargs, tuple_iterator_arg& targs)
{
    if (pargs.at_end() && targs.at_end())
    {
        return true;
    }
    if (pargs.type() == nullptr) // nullptr == wildcard
    {
        // perform submatching
        pargs.next();
        if (pargs.at_end())
        {
            // always true at the end of the pattern
            return true;
        }
        auto pargs_copy = pargs;
        std::vector<size_t> mv;
        auto mv_ptr = (targs.mapping) ? &mv : nullptr;
        // iterate over tu_args until we found a match
        while (targs.at_end() == false)
        {
            tuple_iterator_arg targs_copy(targs.iter, mv_ptr);
            if (do_match(pargs_copy, targs_copy))
            {
                if (mv_ptr)
                {
                    targs.mapping->insert(targs.mapping->end(),
                                          mv.begin(),
                                          mv.end());
                }
                return true;
            }
            // next iteration
            mv.clear();
            targs.next();
        }
    }
    else
    {
        // compare types
        if (targs.at_end() == false && pargs.type() == targs.type())
        {
            // compare values if needed
            if (   pargs.has_value() == false
                || pargs.type()->equal(pargs.value(), targs.value()))
            {
                targs.push_mapping();
                // submatch
                return do_match(pargs.next(), targs.next());
            }
        }
    }
    return false;
}

} } // namespace cppa::detail
