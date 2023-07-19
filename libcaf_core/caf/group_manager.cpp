// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/group_manager.hpp"

#include "caf/all.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/local_group_module.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/group.hpp"
#include "caf/message.hpp"
#include "caf/sec.hpp"
#include "caf/serializer.hpp"

namespace caf {

void group_manager::init(actor_system_config& cfg) {
  CAF_LOG_TRACE("");
  mmap_.emplace("local", make_counted<detail::local_group_module>(*system_));
  for (auto& fac : cfg.group_module_factories) {
    auto ptr = group_module_ptr{fac(), false};
    auto name = ptr->name();
    mmap_.emplace(std::move(name), std::move(ptr));
  }
}

void group_manager::start() {
  CAF_LOG_TRACE("");
}

void group_manager::stop() {
  CAF_LOG_TRACE("");
  for (auto& kvp : mmap_)
    kvp.second->stop();
}

group_manager::~group_manager() {
  // nop
}

group_manager::group_manager(actor_system& sys) : system_(&sys) {
  // nop
}

group group_manager::anonymous() {
  CAF_LOG_TRACE("");
  std::string id = "__#";
  id += std::to_string(++ad_hoc_id_);
  return get_local(id);
}

expected<group> group_manager::get(std::string group_uri) {
  CAF_LOG_TRACE(CAF_ARG(group_uri));
  // URI parsing is pretty much a brute-force approach, no actual validation yet
  auto p = group_uri.find(':');
  if (p == std::string::npos)
    return sec::invalid_argument;
  auto group_id = group_uri.substr(p + 1);
  // erase all but the scheme part from the URI and use that as module name
  group_uri.erase(p);
  return get(group_uri, group_id);
}

expected<group> group_manager::get(const std::string& module_name,
                                   const std::string& group_identifier) {
  CAF_LOG_TRACE(CAF_ARG(module_name) << CAF_ARG(group_identifier));
  if (auto mod = get_module(module_name))
    return mod->get(group_identifier);
  std::string error_msg = R"__(no module named ")__";
  error_msg += module_name;
  error_msg += R"__(" found)__";
  return make_error(sec::no_such_group_module, std::move(error_msg));
}

group group_manager::get_local(const std::string& group_identifier) {
  auto result = get("local", group_identifier);
  CAF_ASSERT(result);
  return std::move(*result);
}

group_module_ptr group_manager::get_module(const std::string& x) const {
  if (auto i = mmap_.find(x); i != mmap_.end())
    return i->second;
  else
    return nullptr;
}

} // namespace caf
