// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/default_actor_handle_codec.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system.hpp"
#include "caf/deserializer.hpp"
#include "caf/error.hpp"
#include "caf/error_code.hpp"
#include "caf/log/core.hpp"
#include "caf/node_id.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/sec.hpp"
#include "caf/serializer.hpp"

namespace caf::detail {

namespace {

template <class PointerType>
error_code<sec> load_actor(PointerType& ptr, actor_system* sys, actor_id aid,
                           const node_id& nid) {
  if (sys == nullptr) {
    return error_code{sec::no_context};
  }
  if (sys->node() == nid) {
    ptr = sys->registry().get(aid);
    log::core::debug("fetch actor handle from local actor registry: {}",
                     (ptr ? "found" : "not found"));
    return {};
  }
  // Get or create a proxy for the remote actor.
  if (auto* registry = proxy_registry::current()) {
    ptr = registry->get_or_put(nid, aid);
    return {};
  }
  return error_code{sec::no_proxy_registry};
}

template <class PointerType>
error_code<sec>
save_actor(const PointerType& storage, actor_id aid, const node_id& nid) {
  // Register locally running actors to be able to deserialize them later.
  if (storage) {
    if constexpr (PointerType::has_weak_ptr_semantics) {
      if (auto sptr = storage.lock()) {
        return save_actor(sptr, aid, nid);
      }
    } else {
      auto& sys = storage->system();
      if (nid == sys.node()) {
        sys.registry().put(aid, storage);
      }
    }
  }
  return {};
}

const actor_control_block* ctrl(const strong_actor_ptr& ptr) {
  return ptr.get();
}

const actor_control_block* ctrl(const weak_actor_ptr& ptr) {
  return ptr.ctrl();
}

template <class PointerType>
bool save_text(serializer& sink, const PointerType& ptr) {
  auto aid = actor_id{0};
  auto nid = node_id{};
  if (ptr != nullptr) {
    const auto* cptr = ctrl(ptr);
    aid = cptr->id();
    nid = cptr->node();
  }
  if (!sink.object(ptr).pretty_name("actor").fields(sink.field("id", aid),
                                                    sink.field("node", nid))) {
    return false;
  }
  if (ptr != nullptr) {
    if (auto err = save_actor(ptr, aid, nid); err.valid()) {
      sink.set_error(error{err.value()});
      return false;
    }
  }
  return true;
}

template <class PointerType>
bool load_text(deserializer& source, PointerType& ptr, actor_system* sys) {
  auto aid = actor_id{0};
  auto nid = node_id{};
  if (!source.object(ptr).pretty_name("actor").fields(
        source.field("id", aid), source.field("node", nid)))
    return false;
  if (aid == 0) {
    ptr = nullptr;
    return true;
  }
  if (auto err = load_actor(ptr, sys, aid, nid); err.valid()) {
    source.set_error(error{err.value()});
    return false;
  }
  return true;
}

template <class PointerType>
bool save_binary(serializer& sink, const PointerType& ptr) {
  actor_id aid = 0;
  node_id nid;
  if (ptr != nullptr) {
    const auto* cptr = ctrl(ptr);
    aid = cptr->id();
    nid = cptr->node();
  }
  if (!sink.value(aid))
    return false;
  if (!inspect(sink, nid))
    return false;
  if (ptr != nullptr) {
    if (auto err = save_actor(ptr, aid, nid); err.valid()) {
      sink.set_error(error{err.value()});
      return false;
    }
  }
  return true;
}

template <class PointerType>
bool load_binary(deserializer& source, PointerType& ptr, actor_system* sys) {
  auto aid = actor_id{0};
  auto nid = node_id{};
  if (!source.value(aid))
    return false;
  if (!inspect(source, nid))
    return false;
  if (aid == 0) {
    ptr = nullptr;
    return true;
  }
  if (auto err = load_actor(ptr, sys, aid, nid); err.valid()) {
    source.set_error(error{err.value()});
    return false;
  }
  return true;
}

} // namespace

default_actor_handle_codec::default_actor_handle_codec(
  actor_system& sys) noexcept
  : sys_(&sys) {
  // nop
}

bool default_actor_handle_codec::save(serializer& sink,
                                      const strong_actor_ptr& ptr) {
  if (sink.has_human_readable_format()) {
    return save_text(sink, ptr);
  }
  return save_binary(sink, ptr);
}

bool default_actor_handle_codec::load(deserializer& source,
                                      strong_actor_ptr& ptr) {
  if (source.has_human_readable_format()) {
    return load_text(source, ptr, sys_);
  }
  return load_binary(source, ptr, sys_);
}

bool default_actor_handle_codec::save(serializer& sink,
                                      const weak_actor_ptr& ptr) {
  if (sink.has_human_readable_format()) {
    return save_text(sink, ptr);
  }
  return save_binary(sink, ptr);
}

bool default_actor_handle_codec::load(deserializer& source,
                                      weak_actor_ptr& ptr) {
  if (source.has_human_readable_format()) {
    return load_text(source, ptr, sys_);
  }
  return load_binary(source, ptr, sys_);
}

} // namespace caf::detail
