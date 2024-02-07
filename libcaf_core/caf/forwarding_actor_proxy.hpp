// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/detail/core_export.hpp"

#include <shared_mutex>

namespace caf {

/// Implements a simple proxy forwarding all operations to a manager.
class CAF_CORE_EXPORT forwarding_actor_proxy : public actor_proxy {
public:
  forwarding_actor_proxy(actor_config& cfg, actor dest);

  ~forwarding_actor_proxy() override;

  const char* name() const override;

  bool enqueue(mailbox_element_ptr what, execution_unit* context) override;

  bool add_backlink(abstract_actor* x) override;

  bool remove_backlink(abstract_actor* x) override;

  void kill_proxy(execution_unit* ctx, error rsn) override;

private:
  bool forward_msg(strong_actor_ptr sender, message_id mid, message msg);

  void force_close_mailbox() final;

  mutable std::shared_mutex broker_mtx_;
  actor broker_;
};

} // namespace caf
