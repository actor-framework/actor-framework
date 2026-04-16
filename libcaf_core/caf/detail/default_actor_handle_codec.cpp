// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/default_actor_handle_codec.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/deserializer.hpp"
#include "caf/serializer.hpp"

namespace caf::detail {

namespace {

bool save_text(serializer& sink, const strong_actor_ptr& ptr) {
  auto aid = actor_id{0};
  auto nid = node_id{};
  if (ptr != nullptr) {
    aid = ptr->id();
    nid = ptr->node();
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

bool load_text(deserializer& source, strong_actor_ptr& ptr, actor_system* sys) {
  auto aid = actor_id{0};
  auto nid = node_id{};
  if (!source.object(ptr).pretty_name("actor").fields(
        source.field("id", aid), source.field("node", nid)))
    return false;
  if (aid == 0 || !nid) {
    ptr = nullptr;
  } else if (auto err = load_actor(ptr, sys, aid, nid); err.valid()) {
    source.set_error(error{err.value()});
    return false;
  }
  return true;
}

bool save_binary(serializer& sink, const strong_actor_ptr& ptr) {
  actor_id aid = 0;
  node_id nid;
  if (ptr != nullptr) {
    aid = ptr->id();
    nid = ptr->node();
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

bool load_binary(deserializer& source, strong_actor_ptr& ptr,
                 actor_system* sys) {
  auto aid = actor_id{0};
  auto nid = node_id{};
  if (!source.value(aid))
    return false;
  if (!inspect(source, nid))
    return false;
  if (aid == 0) {
    ptr = nullptr;
  } else if (auto err = load_actor(ptr, sys, aid, nid); err.valid()) {
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

} // namespace caf::detail
