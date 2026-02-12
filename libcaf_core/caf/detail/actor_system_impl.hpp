// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/telemetry/actor_metrics.hpp"
#include "caf/timespan.hpp"

#include <memory>
#include <span>
#include <string_view>

namespace caf::detail {

/// Abstract base type for actor system implementations.
class CAF_CORE_EXPORT actor_system_impl {
public:
  // -- constructors, destructors, and assignment operators --------------------

  actor_system_impl() = default;

  virtual ~actor_system_impl();

  actor_system_impl(const actor_system_impl&) = delete;

  actor_system_impl& operator=(const actor_system_impl&) = delete;

  virtual telemetry::actor_metrics make_actor_metrics(std::string_view name)
    = 0;

  virtual void start(actor_system& owner) = 0;

  virtual void stop() = 0;

  virtual void thread_started(thread_owner owner) = 0;

  virtual void thread_terminates() = 0;

  virtual size_t inc_running_actors_count(actor_id who) = 0;

  virtual size_t dec_running_actors_count(actor_id who) = 0;

  virtual void
  await_running_actors_count_equal(size_t expected, timespan timeout)
    = 0;

  virtual detail::global_meta_objects_guard_type
  meta_objects_guard() const noexcept
    = 0;

  virtual actor_system_config& config() = 0;

  virtual const actor_system_config& config() const = 0;

  virtual actor_clock& clock() noexcept = 0;

  virtual size_t detached_actors() const noexcept = 0;

  virtual bool await_actors_before_shutdown() const = 0;

  virtual void await_actors_before_shutdown(bool new_value) = 0;

  virtual telemetry::metric_registry& metrics() noexcept = 0;

  virtual const telemetry::metric_registry& metrics() const noexcept = 0;

  virtual const node_id& node() const = 0;

  virtual caf::scheduler& scheduler() = 0;

  virtual caf::logger& logger() = 0;

  virtual actor_registry& registry() = 0;

  virtual std::span<std::unique_ptr<actor_system_module>> modules() = 0;

  virtual actor_id next_actor_id() = 0;

  virtual actor_id latest_actor_id() const = 0;

  virtual size_t running_actors_count() const = 0;

  virtual detail::private_thread* acquire_private_thread() = 0;

  virtual void release_private_thread(detail::private_thread*) = 0;

  virtual detail::mailbox_factory* mailbox_factory() = 0;

  virtual void
  redirect_text_output(void* out,
                       void (*write)(void*, term, const char*, size_t),
                       void (*cleanup)(void*))
    = 0;

  virtual void do_print(term color, const char* buf, size_t num_bytes) = 0;

  virtual void set_node(node_id id) = 0;

  virtual void message_rejected(abstract_actor*) = 0;

  virtual void
  launch(local_actor* ptr, caf::scheduler* ctx, spawn_options options)
    = 0;
};

} // namespace caf::detail
