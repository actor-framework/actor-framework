// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/requirement_error.hpp"

#include "caf/detail/format.hpp"

namespace caf::test {

std::string requirement_error::message() const {
  switch (code_) {
    case code::failed:
      return detail::format("requirement failed at {}:{}", loc_.file_name(),
                            static_cast<int>(loc_.line()));
    default:
      return detail::format("requirement error at {}:{}", loc_.file_name(),
                            static_cast<int>(loc_.line()));
  }
}

[[noreturn]] void
requirement_error::raise_impl(requirement_error::code what,
                              const detail::source_location& loc) {
#ifdef CAF_ENABLE_EXCEPTIONS
  throw requirement_error{what, loc};
#else
  auto msg = requirement_error{what, loc}.message();
  fprintf(stderr, "[FATAL] critical error: %s\n", msg.c_str());
  abort();
#endif
}

} // namespace caf::test
