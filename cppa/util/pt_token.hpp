#ifndef PT_TOKEN_HPP
#define PT_TOKEN_HPP

#include "cppa/primitive_type.hpp"

namespace cppa { namespace util {

/**
 * @brief Achieves static call dispatch (int-to-type idiom).
 */
template<primitive_type PT>
struct pt_token { static const primitive_type value = PT; };

} } // namespace cppa::util

#endif // PT_TOKEN_HPP
