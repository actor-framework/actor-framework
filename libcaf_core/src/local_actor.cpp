// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/local_actor.hpp"

#include <condition_variable>
#include <string>

#include "caf/actor_cast.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/default_attachable.hpp"
#include "caf/detail/glob_match.hpp"
#include "caf/disposable.hpp"
#include "caf/exit_reason.hpp"
#include "caf/logger.hpp"
#include "caf/resumable.hpp"
#include "caf/scheduler.hpp"
#include "caf/sec.hpp"

namespace caf {

namespace {

local_actor::metrics_t make_instance_metrics(local_actor* self) {
  const auto& sys = self->home_system();
  const auto& includes = sys.metrics_actors_includes();
  const auto& excludes = sys.metrics_actors_excludes();
  const auto* name = self->name();
  auto matches = [name](const std::string& glob) {
    return detail::glob_match(name, glob.c_str());
  };
  if (includes.empty()
      || std::none_of(includes.begin(), includes.end(), matches)
      || std::any_of(excludes.begin(), excludes.end(), matches))
    return {
      nullptr,
      nullptr,
      nullptr,
    };
  self->setf(abstract_actor::collects_metrics_flag);
  const auto& families = sys.actor_metric_families();
  string_view sv{name, strlen(name)};
  return {
    families.processing_time->get_or_add({{"name", sv}}),
    families.mailbox_time->get_or_add({{"name", sv}}),
    families.mailbox_size->get_or_add({{"name", sv}}),
  };
}

} // namespace

local_actor::local_actor(actor_config& cfg)
  : monitorable_actor(cfg),
    context_(cfg.host),
    current_element_(nullptr),
    initial_behavior_fac_(std::move(cfg.init_fun)) {
  // nop
}

local_actor::~local_actor() {
  // nop
}

void local_actor::on_destroy() {
  CAF_PUSH_AID_FROM_PTR(this);
#ifdef CAF_ENABLE_ACTOR_PROFILER
  system().profiler_remove_actor(*this);
#endif
  if (!getf(is_cleaned_up_flag)) {
    on_exit();
    cleanup(exit_reason::unreachable, nullptr);
    monitorable_actor::on_destroy();
  }
}

void local_actor::setup_metrics() {
  metrics_ = make_instance_metrics(this);
}

auto local_actor::now() const noexcept -> clock_type::time_point {
  return clock().now();
}

disposable local_actor::request_response_timeout(timespan timeout,
                                                 message_id mid) {
  CAF_LOG_TRACE(CAF_ARG(timeout) << CAF_ARG(mid));
  if (timeout == infinite)
    return {};
  auto t = clock().now() + timeout;
  return clock().schedule_message(
    t, strong_actor_ptr{ctrl()},
    make_mailbox_element(nullptr, mid.response_id(), {},
                         make_error(sec::request_timeout)));
}

void local_actor::monitor(abstract_actor* ptr, message_priority priority) {
  if (ptr != nullptr)
    ptr->attach(
      default_attachable::make_monitor(ptr->address(), address(), priority));
}

void local_actor::monitor(const node_id& node) {
  system().monitor(node, address());
}

void local_actor::demonitor(const actor_addr& whom) {
  CAF_LOG_TRACE(CAF_ARG(whom));
  demonitor(actor_cast<strong_actor_ptr>(whom));
}

void local_actor::demonitor(const strong_actor_ptr& whom) {
  CAF_LOG_TRACE(CAF_ARG(whom));
  if (whom) {
    default_attachable::observe_token tk{address(),
                                         default_attachable::monitor};
    whom->get()->detach(tk);
  }
}

void local_actor::demonitor(const node_id& node) {
  system().demonitor(node, address());
}

void local_actor::on_exit() {
  // nop
}

message_id local_actor::new_request_id(message_priority mp) {
  auto result = ++last_request_id_;
  return mp == message_priority::normal ? result : result.with_high_priority();
}

void local_actor::send_exit(const actor_addr& whom, error reason) {
  send_exit(actor_cast<strong_actor_ptr>(whom), std::move(reason));
}

void local_actor::send_exit(const strong_actor_ptr& dest, error reason) {
  if (!dest)
    return;
  dest->get()->eq_impl(make_message_id(), nullptr, context(),
                       exit_msg{address(), std::move(reason)});
}

const char* local_actor::name() const {
  return "user.local-actor";
}

error local_actor::save_state(serializer&, const unsigned int) {
  CAF_RAISE_ERROR("local_actor::serialize called");
}

error local_actor::load_state(deserializer&, const unsigned int) {
  CAF_RAISE_ERROR("local_actor::deserialize called");
}

void local_actor::initialize() {
  CAF_LOG_TRACE(CAF_ARG2("id", id()) << CAF_ARG2("name", name()));
}

bool local_actor::cleanup(error&& fail_state, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(fail_state));
  // tell registry we're done
  unregister_from_system();
  CAF_LOG_TERMINATE_EVENT(this, fail_state);
  monitorable_actor::cleanup(std::move(fail_state), host);
  return true;
}

} // namespace caf
