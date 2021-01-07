// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <functional>
#include <mutex>
#include <set>
#include <unordered_map>

#include "caf/abstract_group.hpp"
#include "caf/group_module.hpp"

namespace caf::detail {

/// Simple group implementation that allows arbitrary strings as group names.
/// However, the group names must use a string representation of the local node
/// ID as prefix. Each group instance spins up an intermediary actor to enable
/// remote access to the group. The default module is used internally by the
/// "local" as well as the "remote" module.
class CAF_CORE_EXPORT local_group_module : public group_module {
public:
  using super = group_module;

  struct intermediary_actor_state {
    static inline const char* name = "caf.detail.group-intermediary";

    intermediary_actor_state(event_based_actor* self, abstract_group_ptr gptr);

    behavior make_behavior();

    event_based_actor* self;

    abstract_group_ptr gptr;
  };

  /// A group intermediary enables remote actors to join and leave groups on
  /// this endpoint as well as sending message to it.
  using intermediary_actor = stateful_actor<intermediary_actor_state>;

  /// Implementation of the group interface for instances of this module.
  class CAF_CORE_EXPORT impl : public abstract_group {
  public:
    friend local_group_module;

    using super = abstract_group;

    using subscriber_set = std::set<strong_actor_ptr, std::less<>>;

    impl(group_module_ptr mod, std::string id, node_id origin);

    impl(group_module_ptr mod, std::string id);

    ~impl() override;

    void enqueue(strong_actor_ptr sender, message_id mid, message content,
                 execution_unit* host) override;

    bool subscribe(strong_actor_ptr who) override;

    void unsubscribe(const actor_control_block* who) override;

    actor intermediary() const noexcept override;

    void stop() override;

  protected:
    template <class F>
    auto critical_section(F&& fun) const {
      std::unique_lock<std::mutex> guard{mtx_};
      return fun();
    }

    /// @pre `mtx_` is locked
    std::pair<bool, size_t> subscribe_impl(strong_actor_ptr who);

    /// @pre `mtx_` is locked
    std::pair<bool, size_t> unsubscribe_impl(const actor_control_block* who);

    mutable std::mutex mtx_;
    actor intermediary_;
    bool stopped_ = false;
    subscriber_set subscribers_;
  };

  using instances_map = std::unordered_map<std::string, intrusive_ptr<impl>>;

  explicit local_group_module(actor_system& sys);

  ~local_group_module() override;

  expected<group> get(const std::string& group_name) override;

  void stop() override;

private:
  std::mutex mtx_;
  bool stopped_ = false;
  instances_map instances_;
};

} // namespace caf::detail
