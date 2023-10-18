#define CAF_LOG_COMPONENT "app"

#include "caf/caf_main.hpp"
#include "caf/logger.hpp"

#include <string_view>

constexpr std::string_view component = "app";

void foo([[maybe_unused]] int value, bool use_legacy_api) {
  if (use_legacy_api) {
    CAF_LOG_TRACE(CAF_ARG(value));
    CAF_LOG_DEBUG("this is a debug message");
    CAF_LOG_INFO("this is an info message");
    CAF_LOG_WARNING("this is a warning message");
    CAF_LOG_ERROR("this is an error message");
  } else {
    auto trace_guard = caf::logger::trace(component, "value = {}", value);
    caf::logger::debug(component, "this is a debug message");
    caf::logger::info(component, "this is an info message");
    caf::logger::warning(component, "this is a warning message");
    caf::logger::error(component, "this is an error message");
  }
}

class config : public caf::actor_system_config {
public:
  config() {
    opt_group{custom_options_, "global"} //
      .add<std::string>("api", "sets the API");
  }
};

void caf_main(caf::actor_system&, const config& cfg) {
  foo(42, get_or(cfg, "api", "default") == "legacy");
}

// creates a main function for us that calls our caf_main
CAF_MAIN()
