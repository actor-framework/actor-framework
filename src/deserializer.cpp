#include <string>

#include "cppa/object.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace cppa {

deserializer::~deserializer()
{
}

deserializer& operator>>(deserializer& d, object& what)
{
    std::string tname = d.peek_object();
    auto mtype = uniform_type_info::from(tname);
    if (mtype == nullptr)
    {
        throw std::logic_error("no uniform type info found for " + tname);
    }
    what = std::move(mtype->deserialize(&d));
    return d;
}

} // namespace cppa
