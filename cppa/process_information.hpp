#ifndef PROCESS_INFORMATION_HPP
#define PROCESS_INFORMATION_HPP

#include <array>
#include <string>
#include <cstdint>

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/util/comparable.hpp"

namespace cppa {

/**
 * @brief Identifies a process.
 */
class process_information : public ref_counted,
                            util::comparable<process_information>
{

    typedef ref_counted super;

 public:

    static constexpr size_t node_id_size = 20;

    typedef std::array<std::uint8_t, node_id_size> node_id_type;

    //process_information();

    process_information(std::uint32_t process_id, node_id_type const& node_id);

    process_information(std::uint32_t process_id, std::string const& hash);

    process_information(process_information const& other);

    /**
     * @brief Identifies the running process.
     */
    inline std::uint32_t process_id() const { return m_process_id; }

    /**
     * @brief Identifies the host system.
     *
     * A hash build from the MAC address of the first network device
     * and the serial number from the root HD (mounted in "/" or "C:").
     */
    inline node_id_type const& node_id() const { return m_node_id; }

    /**
     * @brief Returns the proccess_information for the running process.
     * @returns
     */
    static intrusive_ptr<process_information> const& get();

    // "inherited" from comparable<process_information>
    int compare(process_information const& other) const;

 private:

    std::uint32_t m_process_id;
    node_id_type m_node_id;

};

void node_id_from_string(std::string const& hash,
                         process_information::node_id_type& node_id);

bool equal(std::string const& hash,
           process_information::node_id_type const& node_id);

inline bool equal(process_information::node_id_type const& node_id,
                  std::string const& hash)
{
    return equal(hash, node_id);
}

std::string to_string(process_information const& what);

/**
 * @brief Converts {@link node_id} to an hexadecimal string.
 */
std::string to_string(process_information::node_id_type const& node_id);

/**
 * @brief A smart pointer type that manages instances of
 *        {@link process_information}.
 */
typedef intrusive_ptr<process_information> process_information_ptr;

} // namespace cppa

#endif // PROCESS_INFORMATION_HPP
