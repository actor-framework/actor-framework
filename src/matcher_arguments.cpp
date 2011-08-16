#include "cppa/detail/matcher_arguments.hpp"

namespace cppa { namespace detail {

matcher_arguments::matcher_arguments(util::any_tuple_iterator tuple_iterator,
                                     std::vector<size_t>* mv)
    : iter(tuple_iterator), mapping(mv)
{
}

matcher_arguments::matcher_arguments(const any_tuple& tup,
                                     std::vector<size_t>* mv)
    : iter(tup), mapping(mv)
{
}

} } // namespace cppa::detail
