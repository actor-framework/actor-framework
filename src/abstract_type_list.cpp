#include "cppa/util/abstract_type_list.hpp"

namespace cppa { namespace util {

abstract_type_list::~abstract_type_list()
{
}

abstract_type_list::const_iterator abstract_type_list::end() const
{
    return const_iterator(nullptr);
}

abstract_type_list::abstract_iterator::~abstract_iterator()
{
}

} } // namespace cppa::util
