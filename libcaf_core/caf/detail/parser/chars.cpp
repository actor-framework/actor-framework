// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/parser/chars.hpp"

namespace caf::detail::parser {

const char whitespace_chars[7] = " \f\n\r\t\v";

const char alphanumeric_chars[63] = "0123456789"
                                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz";

const char alphabetic_chars[53] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "abcdefghijklmnopqrstuvwxyz";

const char hexadecimal_chars[23] = "0123456789ABCDEFabcdef";

const char decimal_chars[11] = "0123456789";

const char octal_chars[9] = "01234567";

const char quote_marks[3] = "\"'";

} // namespace caf::detail::parser
