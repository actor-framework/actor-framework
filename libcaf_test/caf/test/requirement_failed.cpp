// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/requirement_failed.hpp"

#include "caf/detail/format.hpp"

namespace caf::test {

std::string requirement_failed::message() const {
  return detail::format("requirement failed at {}:{}", loc_.file_name(),
                        loc_.line());
}

[[noreturn]] void
requirement_failed::raise_impl(const detail::source_location& loc) {
#ifdef CAF_ENABLE_EXCEPTIONS
  throw requirement_failed{loc};
#else
  auto msg = requirement_failed{loc}.message();
  fprintf(stderr, "[FATAL] critical error: %s\n", msg.c_str());
  abort();
#endif
}

} // namespace caf::test
