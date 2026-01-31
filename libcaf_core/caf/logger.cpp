// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/logger.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/chunked_string.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/current_actor.hpp"
#include "caf/detail/format.hpp"
#include "caf/detail/get_process_id.hpp"
#include "caf/detail/log_level_map.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/detail/sync_ring_buffer.hpp"
#include "caf/local_actor.hpp"
#include "caf/log/core.hpp"
#include "caf/log/level.hpp"
#include "caf/make_counted.hpp"
#include "caf/message.hpp"
#include "caf/term.hpp"
#include "caf/thread_owner.hpp"
#include "caf/timestamp.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <thread>
#include <unordered_map>
#include <utility>

namespace caf {

namespace {

// Stores a pointer to the system-wide logger.
thread_local intrusive_ptr<logger> current_logger_ptr;

} // namespace

logger::line_builder::line_builder() {
  // nop
}

logger::line_builder&&
logger::line_builder::operator<<(const local_actor* self) && {
  return std::move(*this) << self->name();
}

logger::line_builder&&
logger::line_builder::operator<<(const std::string& str) && {
  return std::move(*this) << str.c_str();
}

logger::line_builder&&
logger::line_builder::operator<<(std::string_view str) && {
  if (!str_.empty() && str_.back() != ' ')
    str_ += " ";
  str_.insert(str_.end(), str.begin(), str.end());
  return std::move(*this);
}

logger::line_builder&& logger::line_builder::operator<<(const char* str) && {
  if (!str_.empty() && str_.back() != ' ')
    str_ += " ";
  str_ += str;
  return std::move(*this);
}

logger::line_builder&& logger::line_builder::operator<<(char x) && {
  const char buf[] = {x, '\0'};
  return std::move(*this) << buf;
}

logger::~logger() {
  // nop
}

void logger::legacy_api_log(unsigned level, std::string_view component,
                            std::string msg, std::source_location loc) {
  do_log(log::event::make(level, component, loc, thread_local_aid(), msg));
}

actor_id logger::thread_local_aid() {
  if (auto* ptr = detail::current_actor()) {
    return ptr->id();
  }
  return 0;
}

logger* logger::current_logger() {
  return current_logger_ptr.get();
}

void logger::current_logger(actor_system* sys) {
  if (sys != nullptr)
    current_logger_ptr.reset(&sys->logger(), add_ref);
  else
    current_logger_ptr.reset();
}

void logger::current_logger(logger* ptr) {
  if (ptr != nullptr)
    current_logger_ptr.reset(ptr, add_ref);
  else
    current_logger_ptr.reset();
}

void logger::current_logger(std::nullptr_t) {
  current_logger_ptr.reset();
}

} // namespace caf
