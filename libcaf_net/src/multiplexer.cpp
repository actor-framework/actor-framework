/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/net/multiplexer.hpp"

#include "caf/config.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/logger.hpp"
#include "caf/net/operation.hpp"
#include "caf/net/pollset_updater.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/sec.hpp"
#include "caf/variant.hpp"

#ifndef CAF_WINDOWS
#  include <poll.h>
#else
#  include "caf/detail/socket_sys_includes.hpp"
#endif // CAF_WINDOWS

namespace caf {
namespace net {

#ifndef POLLRDHUP
#  define POLLRDHUP POLLHUP
#endif

#ifndef POLLPRI
#  define POLLPRI POLLIN
#endif

namespace {

#ifdef CAF_WINDOWS
// From the MSDN: If the POLLPRI flag is set on a socket for the Microsoft
//                Winsock provider, the WSAPoll function will fail.
const short input_mask = POLLIN;
#else
const short input_mask = POLLIN | POLLPRI;
#endif

const short error_mask = POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;

const short output_mask = POLLOUT;

short to_bitmask(operation op) {
  switch (op) {
    case operation::read:
      return input_mask;
    case operation::write:
      return output_mask;
    case operation::read_write:
      return input_mask | output_mask;
    default:
      return 0;
  }
}

} // namespace

multiplexer::multiplexer() {
  // nop
}

multiplexer::~multiplexer() {
  // nop
}

error multiplexer::init() {
  auto pipe_handles = make_pipe();
  if (!pipe_handles)
    return std::move(pipe_handles.error());
  tid_ = std::this_thread::get_id();
  add(make_counted<pollset_updater>(pipe_handles->first, shared_from_this()));
  write_handle_ = pipe_handles->second;
  return none;
}

size_t multiplexer::num_socket_managers() const noexcept {
  return managers_.size();
}

ptrdiff_t multiplexer::index_of(const socket_manager_ptr& mgr) {
  auto max_index = static_cast<ptrdiff_t>(managers_.size());
  for (ptrdiff_t index = 0; index < max_index; ++index)
    if (managers_[index] == mgr)
      return index;
  return -1;
}

void multiplexer::update(const socket_manager_ptr& mgr) {
  if (std::this_thread::get_id() == tid_) {
    auto i = std::find(dirty_managers_.begin(), dirty_managers_.end(), mgr);
    if (i == dirty_managers_.end())
      dirty_managers_.emplace_back(mgr);
  } else {
    mgr->ref();
    auto value = reinterpret_cast<intptr_t>(mgr.get());
    variant<size_t, sec> res;
    { // Lifetime scope of guard.
      std::lock_guard<std::mutex> guard{write_lock_};
      res = write(write_handle_, &value, sizeof(intptr_t));
    }
    if (holds_alternative<sec>(res))
      mgr->deref();
  }
}

bool multiplexer::poll_once(bool blocking) {
  // We'll call poll() until poll() succeeds or fails.
  for (;;) {
    int presult;
#ifdef CAF_WINDOWS
    presult = ::WSAPoll(pollset_.data(), static_cast<ULONG>(pollset_.size()),
                        blocking ? -1 : 0);
#else
    presult = ::poll(pollset_.data(), static_cast<nfds_t>(pollset_.size()),
                     blocking ? -1 : 0);
#endif
    if (presult < 0) {
      switch (last_socket_error()) {
        case std::errc::interrupted: {
          // A signal was caught. Simply try again.
          CAF_LOG_DEBUG("received errc::interrupted, try again");
          break;
        }
        case std::errc::not_enough_memory: {
          CAF_LOG_ERROR("poll() failed due to insufficient memory");
          // There's not much we can do other than try again in hope someone
          // else releases memory.
          break;
        }
        default: {
          // Must not happen.
          perror("poll() failed");
          CAF_CRITICAL("poll() failed");
        }
      }
      // Rinse and repeat.
      continue;
    }
    CAF_LOG_DEBUG("poll() on" << pollset_.size() << "sockets reported"
                              << presult << "event(s)");
    // No activity.
    if (presult == 0)
      return false;
    // Scan pollset for events.
    CAF_LOG_DEBUG("scan pollset for socket events");
    for (size_t i = 0; i < pollset_.size() && presult > 0; ++i) {
      auto& x = pollset_[i];
      if (x.revents != 0) {
        handle(managers_[i], x.revents);
        --presult;
      }
    }
    handle_updates();
    return true;
  }
}

void multiplexer::handle_updates() {
  for (auto mgr : dirty_managers_) {
    auto index = index_of(mgr.get());
    if (index == -1) {
      add(std::move(mgr));
    } else {
      // Update or remove an existing manager in the pollset.
      if (mgr->mask() == operation::none) {
        pollset_.erase(pollset_.begin() + index);
        managers_.erase(managers_.begin() + index);
      } else {
        pollset_[index].events = to_bitmask(mgr->mask());
      }
    }
  }
  dirty_managers_.clear();
}

void multiplexer::handle(const socket_manager_ptr& mgr, int mask) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle()) << CAF_ARG(mask));
  CAF_ASSERT(mgr != nullptr);
  bool checkerror = true;
  if ((mask & input_mask) != 0) {
    checkerror = false;
    if (!mgr->handle_read_event())
      mgr->mask_del(operation::read);
  }
  if ((mask & output_mask) != 0) {
    checkerror = false;
    if (!mgr->handle_write_event())
      mgr->mask_del(operation::write);
  }
  if (checkerror && ((mask & error_mask) != 0)) {
    if (mask & POLLNVAL)
      mgr->handle_error(sec::socket_invalid);
    else if (mask & POLLHUP)
      mgr->handle_error(sec::socket_disconnected);
    else
      mgr->handle_error(sec::socket_operation_failed);
    mgr->mask_del(operation::read_write);
  }
}

void multiplexer::add(socket_manager_ptr mgr) {
  pollfd new_entry{socket_cast<socket_id>(mgr->handle()),
                   to_bitmask(mgr->mask()), 0};
  pollset_.emplace_back(new_entry);
  managers_.emplace_back(std::move(mgr));
}

} // namespace net
} // namespace caf
