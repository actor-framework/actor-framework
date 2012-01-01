#ifndef BINARY_DESERIALIZER_HPP
#define BINARY_DESERIALIZER_HPP

#include "cppa/deserializer.hpp"

namespace cppa {

class binary_deserializer : public deserializer
{

    char const* pos;
    char const* end;

    void range_check(size_t read_size);

 public:

    binary_deserializer(char const* buf, size_t buf_size);
    binary_deserializer(char const* begin, char const* end);

    std::string seek_object();
    std::string peek_object();
    void begin_object(std::string const& type_name);
    void end_object();
    size_t begin_sequence();
    void end_sequence();
    primitive_variant read_value(primitive_type ptype);
    void read_tuple(size_t size,
                    primitive_type const* ptypes,
                    primitive_variant* storage);

};

} // namespace cppa

#endif // BINARY_DESERIALIZER_HPP
