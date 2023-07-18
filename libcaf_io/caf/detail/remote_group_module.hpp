// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/fwd.hpp"

#include "caf/detail/group_tunnel.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/fwd.hpp"
#include "caf/group.hpp"
#include "caf/group_module.hpp"

#include <mutex>
#include <string>
#include <unordered_map>

namespace caf::detail {

class CAF_IO_EXPORT remote_group_module : public group_module {
public:
  using super = group_module;

  using instances_map = std::unordered_map<std::string, group_tunnel_ptr>;

  using nodes_map = std::unordered_map<node_id, instances_map>;

  explicit remote_group_module(io::middleman* mm);

  void stop() override;

  expected<group> get(const std::string& group_name) override;

  // Get instance if it exists or create an unconnected tunnel and ask the
  // middleman to connect it lazily.
  group_tunnel_ptr get_impl(const node_id& origin,
                            const std::string& group_name);

  // Get instance if it exists or create a connected tunnel.
  group_tunnel_ptr get_impl(actor intermediary, const std::string& group_name);

  // Get instance if it exists or return `nullptr`.
  group_tunnel_ptr lookup(const node_id& origin, const std::string& group_name);

private:
  template <class F>
  auto critical_section(F&& fun) {
    std::unique_lock<std::mutex> guard{mtx_};
    return fun();
  }

  // Stops an instance and removes it from this module.
  void drop(const group_tunnel_ptr& instance);

  // Connects an instance when it is still associated to this module.
  void connect(const group_tunnel_ptr& instance, actor intermediary);

  std::function<void(actor)> make_callback(const group_tunnel_ptr& instance);

  // Note: the actor system stops the group module before shutting down the
  //       middleman. Hence, it's safe to hold onto a raw pointer here.
  io::middleman* mm_;
  std::mutex mtx_;
  bool stopped_ = false;
  nodes_map nodes_;
};

using remote_group_module_ptr = intrusive_ptr<remote_group_module>;

} // namespace caf::detail
