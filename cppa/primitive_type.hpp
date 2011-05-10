#ifndef PRIMITIVE_TYPE_HPP
#define PRIMITIVE_TYPE_HPP

namespace cppa {

/**
 * @brief Includes integers (signed and unsigned), floating points
 *        and unicode strings (std::string, std::u16string and std::u32string).
 */
enum primitive_type
{
    pt_int8,        pt_int16,       pt_int32,        pt_int64,
    pt_uint8,       pt_uint16,      pt_uint32,       pt_uint64,
    pt_float,       pt_double,      pt_long_double,
    pt_u8string,    pt_u16string,   pt_u32string,
    pt_null
};

constexpr const char* primitive_type_names[] =
{
    "pt_int8",        "pt_int16",       "pt_int32",       "pt_int64",
    "pt_uint8",       "pt_uint16",      "pt_uint32",      "pt_uint64",
    "pt_float",       "pt_double",      "pt_long_double",
    "pt_u8string",    "pt_u16string",   "pt_u32string",
    "pt_null"
};

constexpr const char* primitive_type_name(primitive_type ptype)
{
    return primitive_type_names[static_cast<int>(ptype)];
}

} // namespace cppa

#endif // PRIMITIVE_TYPE_HPP
