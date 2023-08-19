// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/intrusive/singly_linked.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/tracing_data.hpp"

#include <chrono>
#include <cstddef>
#include <memory>

namespace caf {

class CAF_CORE_EXPORT mailbox_element
  : public intrusive::singly_linked<mailbox_element> {
public:
  using forwarding_stack = std::vector<strong_actor_ptr>;

  /// Source of this message and receiver of the final response.
  strong_actor_ptr sender;

  /// Denotes whether this an asynchronous message or a request.
  message_id mid;

  /// `stages.back()` is the next actor in the forwarding chain,
  /// if this is empty then the original sender receives the response.
  forwarding_stack stages;

#ifdef CAF_ENABLE_ACTOR_PROFILER
  /// Optional tracing information. This field is unused by default, but an
  /// @ref actor_profiler can make use of it to inject application-specific
  /// instrumentation.
  tracing_data_ptr tracing_id;
#endif // CAF_ENABLE_ACTOR_PROFILER

  /// Stores the payload.
  message payload;

  /// Stores a timestamp for when this element got enqueued.
  std::chrono::steady_clock::time_point enqueue_time;

  /// Sets `enqueue_time` to the current time.
  void set_enqueue_time() {
    enqueue_time = std::chrono::steady_clock::now();
  }

  /// Returns the time between enqueueing the message and `t`.
  double seconds_until(std::chrono::steady_clock::time_point t) const {
    namespace ch = std::chrono;
    using fractional_seconds = ch::duration<double>;
    return ch::duration_cast<fractional_seconds>(t - enqueue_time).count();
  }

  /// Returns the time since calling `set_enqueue_time` in seconds.
  double seconds_since_enqueue() const {
    namespace ch = std::chrono;
    return seconds_until(std::chrono::steady_clock::now());
  }

  mailbox_element() = default;

  mailbox_element(strong_actor_ptr sender, message_id mid,
                  forwarding_stack stages, message payload);

  bool is_high_priority() const {
    return mid.category() == message_id::urgent_message_category;
  }

  mailbox_element(mailbox_element&&) = delete;
  mailbox_element(const mailbox_element&) = delete;
  mailbox_element& operator=(mailbox_element&&) = delete;
  mailbox_element& operator=(const mailbox_element&) = delete;

  // -- backward compatibility -------------------------------------------------

  message& content() noexcept {
    return payload;
  }

  const message& content() const noexcept {
    return payload;
  }
};

/// @relates mailbox_element
template <class Inspector>
bool inspect(Inspector& f, mailbox_element& x) {
  return f.object(x).fields(f.field("sender", x.sender), f.field("mid", x.mid),
                            f.field("stages", x.stages),
#ifdef CAF_ENABLE_ACTOR_PROFILER
                            f.field("tracing_id", x.tracing_id),
#endif // CAF_ENABLE_ACTOR_PROFILER
                            f.field("payload", x.payload));
}

/// @relates mailbox_element
using mailbox_element_ptr = std::unique_ptr<mailbox_element>;

/// @relates mailbox_element
CAF_CORE_EXPORT mailbox_element_ptr
make_mailbox_element(strong_actor_ptr sender, message_id id,
                     mailbox_element::forwarding_stack stages, message content);

/// @relates mailbox_element
template <class T, class... Ts>
std::enable_if_t<!std::is_same_v<typename std::decay_t<T>, message>
                   || (sizeof...(Ts) > 0),
                 mailbox_element_ptr>
make_mailbox_element(strong_actor_ptr sender, message_id id,
                     mailbox_element::forwarding_stack stages, T&& x,
                     Ts&&... xs) {
  return make_mailbox_element(std::move(sender), id, std::move(stages),
                              make_message(std::forward<T>(x),
                                           std::forward<Ts>(xs)...));
}

} // namespace caf
