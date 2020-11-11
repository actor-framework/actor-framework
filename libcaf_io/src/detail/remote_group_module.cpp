/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/remote_group_module.hpp"

#include "caf/detail/group_tunnel.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/io/middleman.hpp"
#include "caf/make_counted.hpp"
#include "caf/sec.hpp"

namespace caf::detail {

remote_group_module::remote_group_module(io::middleman* mm)
  : super(mm->system(), "remote"), mm_(mm) {
  // nop
}

void remote_group_module::stop() {
  nodes_map tmp;
  critical_section([this, &tmp] {
    using std::swap;
    if (!stopped_) {
      stopped_ = true;
      swap(nodes_, tmp);
    }
  });
  for (auto& nodes_kvp : tmp)
    for (auto& instances_kvp : nodes_kvp.second)
      instances_kvp.second.get()->stop();
}

expected<group> remote_group_module::get(const std::string& group_locator) {
  return mm_->remote_group(group_locator);
}

group_tunnel_ptr remote_group_module::get_impl(const node_id& origin,
                                               const std::string& group_name) {
  CAF_ASSERT(origin != none);
  bool lazy_connect = false;
  auto instance = critical_section([&, this] {
    if (stopped_) {
      return group_tunnel_ptr{nullptr};
    } else {
      auto& instances = nodes_[origin];
      if (auto i = instances.find(group_name); i != instances.end()) {
        return i->second;
      } else {
        lazy_connect = true;
        auto instance = make_counted<detail::group_tunnel>(this, group_name,
                                                           origin);
        instances.emplace(group_name, instance);
        return instance;
      }
    }
  });
  if (lazy_connect)
    mm_->resolve_remote_group_intermediary(origin, group_name,
                                           make_callback(instance));
  return instance;
}

group_tunnel_ptr remote_group_module::get_impl(actor intermediary,
                                               const std::string& group_name) {
  CAF_ASSERT(intermediary != nullptr);
  return critical_section([&, this]() {
    if (stopped_) {
      return group_tunnel_ptr{nullptr};
    } else {
      auto& instances = nodes_[intermediary.node()];
      if (auto i = instances.find(group_name); i != instances.end()) {
        auto instance = i->second;
        instance->connect(std::move(intermediary));
        return instance;
      } else {
        auto instance = make_counted<detail::group_tunnel>(
          this, group_name, std::move(intermediary));
        instances.emplace(group_name, instance);
        return instance;
      }
    }
  });
}

group_tunnel_ptr remote_group_module::lookup(const node_id& origin,
                                             const std::string& group_name) {
  return critical_section([&, this] {
    if (auto i = nodes_.find(origin); i != nodes_.end())
      if (auto j = i->second.find(group_name); j != i->second.end())
        return j->second;
    return group_tunnel_ptr{nullptr};
  });
}

void remote_group_module::drop(const group_tunnel_ptr& instance) {
  CAF_ASSERT(instance != nullptr);
  critical_section([&, this] {
    if (auto i = nodes_.find(instance->origin()); i != nodes_.end()) {
      if (auto j = i->second.find(instance->identifier());
          j != i->second.end()) {
        i->second.erase(j);
        if (i->second.empty())
          nodes_.erase(i);
      }
    }
  });
  instance->stop();
}

void remote_group_module::connect(const group_tunnel_ptr& instance,
                                  actor intermediary) {
  CAF_ASSERT(instance != nullptr);
  bool stop_instance = critical_section([&, this] {
    if (auto i = nodes_.find(instance->origin()); i != nodes_.end()) {
      if (auto j = i->second.find(instance->identifier());
          j != i->second.end() && j->second == instance) {
        instance->connect(intermediary);
        return false;
      }
    }
    return true;
  });
  if (stop_instance)
    instance->stop();
}

std::function<void(actor)>
remote_group_module::make_callback(const group_tunnel_ptr& instance) {
  return [instance, strong_this{remote_group_module_ptr{this}}](actor hdl) {
    if (hdl == nullptr)
      strong_this->drop(instance);
    else
      strong_this->connect(instance, std::move(hdl));
  };
}

} // namespace caf::detail
