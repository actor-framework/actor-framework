#include <sstream>

#include "cppa/process_information.hpp"

namespace cppa {

std::string process_information::node_id_as_string() const
{
    std::ostringstream oss;
    oss << std::hex;
    for (size_t i = 0; i < 20; ++i)
    {
        oss.width(2);
        oss.fill('0');
        oss << static_cast<std::uint32_t>(node_id[i]);
    }
    return oss.str();
}

const process_information& process_information::get()
{

}

} // namespace cppa
