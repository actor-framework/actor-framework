#include "caf/test/fixture/deterministic_logger.hpp"

#include "caf/test/reporter.hpp"

#include "caf/actor_system_config.hpp"

#include <string>

using caf::test::reporter;

namespace caf::test::fixture {

void deterministic_logger::do_log(const context& ctx, std::string&& msg) {
  const auto location = caf::detail::source_location::current(ctx.file_name,
                                                              ctx.function_name,
                                                              ctx.line_number);
  switch (ctx.level) {
    case CAF_LOG_LEVEL_ERROR:
      reporter::instance().print_error(std::move(msg), location);
      return;
    case CAF_LOG_LEVEL_WARNING:
      reporter::instance().print_warning(std::move(msg), location);
      return;
    case CAF_LOG_LEVEL_INFO:
      reporter::instance().print_info(std::move(msg), location);
      return;
    case CAF_LOG_LEVEL_DEBUG:
      reporter::instance().print_debug(std::move(msg), location);
      return;
    case CAF_LOG_LEVEL_TRACE:
      return;
  }
}

bool deterministic_logger::accepts(unsigned level, std::string_view) {
  if (level > verbosity_)
    return false;
  return true;
}

void deterministic_logger::init(const actor_system_config& cfg) {
  CAF_IGNORE_UNUSED(cfg);
  verbosity_ = reporter::instance().verbosity();
}

} // namespace caf::test::fixture
