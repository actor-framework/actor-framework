// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/token_based_credit_controller.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/config_value.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/serialized_size.hpp"
#include "caf/local_actor.hpp"
#include "caf/settings.hpp"

namespace caf::detail {

token_based_credit_controller::token_based_credit_controller(local_actor* ptr) {
  namespace fallback = defaults::stream::token_policy;
  // Initialize from the config parameters.
  auto& cfg = ptr->system().config();
  if (auto section = get_if<settings>(&cfg, "caf.stream.token-based-policy")) {
    batch_size_ = get_or(*section, "batch-size", fallback::batch_size);
    buffer_size_ = get_or(*section, "buffer-size", fallback::buffer_size);
  } else {
    batch_size_ = fallback::batch_size;
    buffer_size_ = fallback::buffer_size;
  }
}

token_based_credit_controller::~token_based_credit_controller() {
  // nop
}

void token_based_credit_controller::before_processing(downstream_msg::batch&) {
  // nop
}

credit_controller::calibration token_based_credit_controller::init() {
  return calibrate();
}

credit_controller::calibration token_based_credit_controller::calibrate() {
  return {buffer_size_, batch_size_, std::numeric_limits<int32_t>::max()};
}

} // namespace caf::detail
