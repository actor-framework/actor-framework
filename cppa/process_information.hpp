#ifndef PROCESS_INFORMATION_HPP
#define PROCESS_INFORMATION_HPP

#include <string>
#include <cstdint>

#include "cppa/ref_counted.hpp"
#include "cppa/util/comparable.hpp"

namespace cppa {

/**
 * @brief Identifies a process.
 */
class process_information : public ref_counted,
                            util::comparable<process_information>
{

 public:

    static constexpr size_t node_id_size = 20;

    process_information();

    process_information(const process_information& other);

    process_information& operator=(const process_information& other);

    /**
     * @brief Identifies the running process.
     */
    std::uint32_t process_id;

    /**
     * @brief Identifies the host system.
     *
     * A hash build from the MAC address of the first network device
     * and the serial number from the root HD (mounted in "/" or "C:").
     */
    std::uint8_t node_id[node_id_size];

    /**
     * @brief Converts {@link node_id} to an hexadecimal string.
     */
    std::string node_id_as_string() const;

    /**
     * @brief Returns the proccess_information for the running process.
     * @return
     */
    static const process_information& get();

    // "inherited" from comparable<process_information>
    int compare(const process_information& other) const;

};

} // namespace cppa

#endif // PROCESS_INFORMATION_HPP
