#include "cppa/util/duration.hpp"

namespace {

inline std::uint64_t ui64_val(const cppa::util::duration& d)
{
    return static_cast<std::uint64_t>(d.unit) * d.count;
}

} // namespace <anonmyous>

namespace cppa { namespace util {

bool operator==(const duration& lhs, const duration& rhs)
{
    return (lhs.unit == rhs.unit ? lhs.count == rhs.count
                                 : ui64_val(lhs) == ui64_val(rhs));
}

} } // namespace cppa::util

namespace boost {

system_time& operator+=(system_time& lhs, const cppa::util::duration& d)
{
    switch (d.unit)
    {
        case cppa::util::time_unit::seconds:
            lhs += posix_time::seconds(d.count);
            break;

        case cppa::util::time_unit::milliseconds:
            lhs += posix_time::milliseconds(d.count);
            break;

        case cppa::util::time_unit::microseconds:
            lhs += posix_time::microseconds(d.count);
            break;

        default: break;
    }
    return lhs;
}

} // namespace boost
