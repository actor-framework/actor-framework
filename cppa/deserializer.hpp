#ifndef DESERIALIZER_HPP
#define DESERIALIZER_HPP

#include <string>
#include <cstddef>

#include "cppa/primitive_type.hpp"
#include "cppa/primitive_variant.hpp"

namespace cppa {

class object;

/**
 * @ingroup Serialize
 * @brief Technology-independent deserialization interface.
 */
class deserializer
{

    deserializer(const deserializer&) = delete;
    deserializer& operator=(const deserializer&) = delete;

 public:

    deserializer() = default;

    virtual ~deserializer();

    /**
     * @brief Seeks the beginning of the next object and return
     *        its uniform type name.
     */
    virtual std::string seek_object() = 0;

    /**
     * @brief Equal to {@link seek_object()} but doesn't
     *        modify the internal in-stream position.
     */
    virtual std::string peek_object() = 0;

    /**
     * @brief Begins deserialization of an object of type @p type_name.
     * @param type_name The platform-independent @p libcppa type name.
     */
    virtual void begin_object(const std::string& type_name) = 0;

    /**
     * @brief Ends deserialization of an object.
     */
    virtual void end_object() = 0;

    /**
     * @brief Begins deserialization of a sequence.
     * @returns The size of the sequence.
     */
    virtual size_t begin_sequence() = 0;

    /**
     * @brief Ends deserialization of a sequence.
     */
    virtual void end_sequence() = 0;

    /**
     * @brief Reads a primitive value from the data source of type @p ptype.
     * @param ptype Expected primitive data type.
     * @returns A primitive value of type @p ptype.
     */
    virtual primitive_variant read_value(primitive_type ptype) = 0;

    /**
     * @brief Reads a tuple of primitive values from the data
     *        source of the types @p ptypes.
     * @param num The size of the tuple.
     * @param ptype Array of expected primitive data types.
     * @param storage Array of size @p num, storing the result of this function.
     */
    virtual void read_tuple(size_t num,
                            const primitive_type* ptypes,
                            primitive_variant* storage   ) = 0;

};

deserializer& operator>>(deserializer& d, object& what);

} // namespace cppa

#endif // DESERIALIZER_HPP
