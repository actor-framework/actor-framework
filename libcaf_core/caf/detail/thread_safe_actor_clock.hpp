// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "caf/abstract_actor.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/ringbuffer.hpp"
#include "caf/detail/simple_actor_clock.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT thread_safe_actor_clock : public simple_actor_clock {
public:
  // -- constants --------------------------------------------------------------

  static constexpr size_t buffer_size = 64;

  // -- member types -----------------------------------------------------------

  using super = simple_actor_clock;

  // -- member functions -------------------------------------------------------

  void set_ordinary_timeout(time_point t, abstract_actor* self,
                            std::string type, uint64_t id) override;

  void set_request_timeout(time_point t, abstract_actor* self,
                           message_id id) override;

  void set_multi_timeout(time_point t, abstract_actor* self, std::string type,
                         uint64_t id) override;

  void cancel_ordinary_timeout(abstract_actor* self, std::string type) override;

  void cancel_request_timeout(abstract_actor* self, message_id id) override;

  void cancel_timeouts(abstract_actor* self) override;

  void schedule_message(time_point t, strong_actor_ptr receiver,
                        mailbox_element_ptr content) override;

  void schedule_message(time_point t, group target, strong_actor_ptr sender,
                        message content) override;

  void cancel_all() override;

  void run_dispatch_loop();

  void cancel_dispatch_loop();

private:
  void push(event* ptr);

  /// Receives timer events from other threads.
  detail::ringbuffer<unique_event_ptr, buffer_size> queue_;

  /// Locally caches events for processing.
  std::array<unique_event_ptr, buffer_size> events_;
};

} // namespace caf::detail
