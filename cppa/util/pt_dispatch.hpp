#ifndef PT_DISPATCH_HPP
#define PT_DISPATCH_HPP

namespace cppa { namespace util {

/**
 * @brief Creates a {@link pt_token} from the runtime value @p ptype
 *        and invokes @p f with this token.
 * @note Does nothing if ptype == pt_null.
 */
template<typename Fun>
void pt_dispatch(primitive_type ptype, Fun&& f)
{
    switch (ptype)
    {
     case pt_int8:        f(pt_token<pt_int8>());        break;
     case pt_int16:       f(pt_token<pt_int16>());       break;
     case pt_int32:       f(pt_token<pt_int32>());       break;
     case pt_int64:       f(pt_token<pt_int64>());       break;
     case pt_uint8:       f(pt_token<pt_uint8>());       break;
     case pt_uint16:      f(pt_token<pt_uint16>());      break;
     case pt_uint32:      f(pt_token<pt_uint32>());      break;
     case pt_uint64:      f(pt_token<pt_uint64>());      break;
     case pt_float:       f(pt_token<pt_float>());       break;
     case pt_double:      f(pt_token<pt_double>());      break;
     case pt_long_double: f(pt_token<pt_long_double>()); break;
     case pt_u8string:    f(pt_token<pt_u8string>());    break;
     case pt_u16string:   f(pt_token<pt_u16string>());   break;
     case pt_u32string:   f(pt_token<pt_u32string>());   break;
     default: break;
    }
}

} } // namespace cppa::util

#endif // PT_DISPATCH_HPP
