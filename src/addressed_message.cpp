#include <algorithm>

#include "cppa/to_string.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/detail/addressed_message.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

namespace cppa { namespace detail {

bool addressed_message::process_ptr_less::operator()
(const process_information_ptr& lhs, const process_information_ptr& rhs)
{
    if (lhs && rhs)
    {
        return lhs.compare(rhs) < 0;
    }
    else if (!lhs && rhs)
    {
        return true;
    }
    return false;
}

bool operator==(const addressed_message& lhs, const addressed_message& rhs)
{
    return    lhs.content() == rhs.content()
           && std::equal(lhs.receivers().begin(),
                         lhs.receivers().end(),
                         rhs.receivers().begin());
}

void addressed_message::serialize_to(serializer* sink) const
{
    primitive_variant ptup[2];
    primitive_variant pval = static_cast<std::uint32_t>(m_receivers.size());
    // size of the map
    sink->write_value(pval);
    for (auto i = m_receivers.begin(); i != m_receivers.end(); ++i)
    {
        if (i->first)
        {
            ptup[0] = i->first->process_id();
            ptup[1] = to_string(i->first->node_id());
            // process id as { process_id, node_id } tuple
            sink->write_tuple(2, ptup);
            // size of the vector that holds the actor ids
            pval = static_cast<std::uint32_t>(i->second.size());
            sink->write_value(pval);
            for (auto j = i->second.begin(); j != i->second.end(); ++j)
            {
                pval = static_cast<std::uint32_t>(*j);
                // actor id of one of the receivers
                sink->write_value(pval);
            }
        }
    }
}

void addressed_message::deserialize_from(deserializer* source)
{
    m_receivers.clear();
    primitive_variant ptup[2];
    primitive_type ptypes[] = { pt_uint32, pt_u8string };
    // size of the map
    auto map_size = get<std::uint32_t>(source->read_value(pt_uint32));
    for (std::uint32_t i = 0; i < map_size; ++i)
    {
        // process id as { process_id, node_id } tuple
        source->read_tuple(2, ptypes, ptup);
        process_information_ptr pi;
        pi.reset(new process_information(get<std::uint32_t>(ptup[0]),
                                         get<std::string>(ptup[1])));
        std::vector<actor_id>& vec = m_receivers[pi];
        // size of vec
        auto vec_size = get<std::uint32_t>(source->read_value(pt_uint32));
        vec.reserve(vec_size);
        // fill vector with the IDs of all receivers
        for (decltype(vec_size) j = 0; j < vec_size; ++j)
        {
            vec.push_back(get<std::uint32_t>(source->read_value(pt_uint32)));
        }
    }
}

} } // namespace cppa::detail
