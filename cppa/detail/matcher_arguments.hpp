#ifndef MATCHER_ARGUMENTS_HPP
#define MATCHER_ARGUMENTS_HPP

#include <vector>

#include "cppa/util/any_tuple_iterator.hpp"

namespace cppa { namespace detail {

struct matcher_arguments
{

    util::any_tuple_iterator iter;
    std::vector<size_t>* mapping;

    matcher_arguments(const any_tuple& tup, std::vector<size_t>* mv = nullptr);

    matcher_arguments(util::any_tuple_iterator tuple_iterator,
                      std::vector<size_t>* mv = nullptr);

    matcher_arguments(const matcher_arguments&) = default;

    matcher_arguments& operator=(const matcher_arguments&) = default;

    inline bool at_end() const;

    inline matcher_arguments& next();

    inline bool push_mapping();

};

inline bool matcher_arguments::at_end() const
{
    return iter.at_end();
}

inline matcher_arguments& matcher_arguments::next()
{
    iter.next();
    return *this;
}

inline bool matcher_arguments::push_mapping()
{
    if (mapping) mapping->push_back(iter.position());
    return true;
}

} } // namespace cppa::detail

#endif // MATCHER_ARGUMENTS_HPP
