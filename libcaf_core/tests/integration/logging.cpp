#define CAF_LOG_COMPONENT "app"

#include "caf/caf_main.hpp"
#include "caf/logger.hpp"

void foo(int value) {
  CAF_LOG_TRACE(CAF_ARG(value));
  CAF_LOG_DEBUG("this is a debug message");
  CAF_LOG_INFO("this is an info message");
  CAF_LOG_WARNING("this is a warning message");
  CAF_LOG_ERROR("this is an error message");
}

void caf_main(caf::actor_system&) {
  foo(42);
}

// creates a main function for us that calls our caf_main
CAF_MAIN()
