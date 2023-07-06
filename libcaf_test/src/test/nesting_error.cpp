// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/nesting_error.hpp"

#include "caf/config.hpp"
#include "caf/detail/format.hpp"

#include <cstdio>

namespace caf::test {

std::string nesting_error::message() const {
  switch (code_) {
    default: // not allowed
      return detail::format("cannot nest a {} in a {} block",
                            macro_name(child_), macro_name(parent_));
    case code::too_many:
      return detail::format("too many {} blocks in a {} block",
                            macro_name(child_), macro_name(parent_));
    case code::invalid_sequence:
      return detail::format("need a {} block before a {} block",
                            macro_name(parent_), macro_name(child_));
  }
}

[[noreturn]] void
nesting_error::raise_impl(nesting_error::code what, block_type parent,
                          block_type child,
                          const detail::source_location& loc) {
#ifdef CAF_ENABLE_EXCEPTIONS
  throw nesting_error{what, parent, child, loc};
#else
  auto msg = nesting_error{code::not_allowed, parent, child}.message();
  fprintf(stderr, "[FATAL] critical error (%s:%d): %s\n", msg.c_str(),
          loc.file_name, static_cast<int>(loc.line));
  abort();
#endif
}

} // namespace caf::test
