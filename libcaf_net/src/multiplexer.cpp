// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/multiplexer.hpp"

#include <algorithm>

#include "caf/byte.hpp"
#include "caf/config.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/operation.hpp"
#include "caf/net/pollset_updater.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/variant.hpp"

#ifndef CAF_WINDOWS
#  include <poll.h>
#else
#  include "caf/detail/socket_sys_includes.hpp"
#endif // CAF_WINDOWS

namespace caf::net {

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

// -- constructors, destructors, and assignment operators ----------------------

multiplexer::multiplexer(middleman* owner) : owner_(owner) {
  // nop
}

multiplexer::~multiplexer() {
  // nop
}

// -- initialization -----------------------------------------------------------

error multiplexer::init() {
  auto pipe_handles = make_pipe();
  if (!pipe_handles)
    return std::move(pipe_handles.error());
  auto updater = make_counted<pollset_updater>(pipe_handles->first, this);
  settings dummy;
  if (auto err = updater->init(dummy))
    return err;
  add(std::move(updater));
  write_handle_ = pipe_handles->second;
  return none;
}

// -- properties ---------------------------------------------------------------

size_t multiplexer::num_socket_managers() const noexcept {
  return managers_.size();
}

ptrdiff_t multiplexer::index_of(const socket_manager_ptr& mgr) {
  auto first = managers_.begin();
  auto last = managers_.end();
  auto i = std::find(first, last, mgr);
  return i == last ? -1 : std::distance(first, i);
}

middleman& multiplexer::owner() {
  CAF_ASSERT(owner_ != nullptr);
  return *owner_;
}

actor_system& multiplexer::system() {
  return owner().system();
}

// -- thread-safe signaling ----------------------------------------------------

void multiplexer::register_reading(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  if (std::this_thread::get_id() == tid_) {
    if (shutting_down_) {
      // discard
    } else if (mgr->mask() != operation::none) {
      CAF_ASSERT(index_of(mgr) != -1);
      if (mgr->mask_add(operation::read)) {
        auto& fd = pollset_[index_of(mgr)];
        fd.events |= input_mask;
      }
    } else if (mgr->mask_add(operation::read)) {
      add(mgr);
    }
  } else {
    write_to_pipe(pollset_updater::register_reading_code, mgr);
  }
}

void multiplexer::register_writing(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  if (std::this_thread::get_id() == tid_) {
    if (shutting_down_) {
      // discard
    } else if (mgr->mask() != operation::none) {
      CAF_ASSERT(index_of(mgr) != -1);
      if (mgr->mask_add(operation::write)) {
        auto& fd = pollset_[index_of(mgr)];
        fd.events |= output_mask;
      }
    } else if (mgr->mask_add(operation::write)) {
      add(mgr);
    }
  } else {
    write_to_pipe(pollset_updater::register_writing_code, mgr);
  }
}

void multiplexer::init(const socket_manager_ptr& mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle().id));
  if (std::this_thread::get_id() == tid_) {
    if (shutting_down_) {
      // discard
    } else {
      if (auto err = mgr->init(content(system().config()))) {
        CAF_LOG_ERROR("mgr->init failed: " << err);
        // The socket manager should not register itself for any events if
        // initialization fails. So there's probably nothing we could do
        // here other than discarding the manager.
      }
    }
  } else {
    write_to_pipe(pollset_updater::init_manager_code, mgr);
  }
}

void multiplexer::close_pipe() {
  CAF_LOG_TRACE("");
  std::lock_guard<std::mutex> guard{write_lock_};
  if (write_handle_ != invalid_socket) {
    close(write_handle_);
    write_handle_ = pipe_socket{};
  }
}

// -- control flow -------------------------------------------------------------

bool multiplexer::poll_once(bool blocking) {
  CAF_LOG_TRACE(CAF_ARG(blocking));
  if (pollset_.empty())
    return false;
  // We'll call poll() until poll() succeeds or fails.
  for (;;) {
    int presult =
#ifdef CAF_WINDOWS
      ::WSAPoll(pollset_.data(), static_cast<ULONG>(pollset_.size()),
                blocking ? -1 : 0);
#else
      ::poll(pollset_.data(), static_cast<nfds_t>(pollset_.size()),
             blocking ? -1 : 0);
#endif
    if (presult > 0) {
      CAF_LOG_DEBUG("poll() on" << pollset_.size() << "sockets reported"
                                << presult << "event(s)");
      // Scan pollset for events.
      CAF_LOG_DEBUG("scan pollset for socket events");
      for (size_t i = 0; i < pollset_.size() && presult > 0;) {
        auto revents = pollset_[i].revents;
        if (revents != 0) {
          auto events = pollset_[i].events;
          auto mgr = managers_[i];
          auto new_events = handle(mgr, events, revents);
          --presult;
          if (new_events == 0) {
            del(i);
            continue;
          } else if (new_events != events) {
            pollset_[i].events = new_events;
          }
        }
        ++i;
      }
      return true;
    } else if (presult == 0) {
      // No activity.
      return false;
    } else {
      auto code = last_socket_error();
      switch (code) {
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
          auto int_code = static_cast<int>(code);
          auto msg = std::generic_category().message(int_code);
          string_view prefix = "poll() failed: ";
          msg.insert(msg.begin(), prefix.begin(), prefix.end());
          CAF_CRITICAL(msg.c_str());
        }
      }
    }
  }
}

void multiplexer::set_thread_id() {
  CAF_LOG_TRACE("");
  tid_ = std::this_thread::get_id();
}

void multiplexer::run() {
  CAF_LOG_TRACE("");
  while (!pollset_.empty())
    poll_once(true);
}

void multiplexer::shutdown() {
  CAF_LOG_TRACE("");
  if (std::this_thread::get_id() == tid_) {
    CAF_LOG_DEBUG("initiate shutdown");
    shutting_down_ = true;
    // First manager is the pollset_updater. Skip it and delete later.
    for (size_t i = 1; i < managers_.size();) {
      auto& mgr = managers_[i];
      if (mgr->mask_del(operation::read)) {
        auto& fd = pollset_[index_of(mgr)];
        fd.events &= ~input_mask;
      }
      if (mgr->mask() == operation::none)
        del(i);
      else
        ++i;
    }
    close_pipe();
  } else {
    CAF_LOG_DEBUG("push shutdown event to pipe");
    write_to_pipe(4, nullptr);
  }
}

// -- utility functions --------------------------------------------------------

short multiplexer::handle(const socket_manager_ptr& mgr, short events,
                          short revents) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle()));
  CAF_ASSERT(mgr != nullptr);
  bool checkerror = true;
  if ((revents & input_mask) != 0) {
    checkerror = false;
    if (!mgr->handle_read_event()) {
      mgr->mask_del(operation::read);
      events &= ~input_mask;
    }
  }
  if ((revents & output_mask) != 0) {
    checkerror = false;
    if (!mgr->handle_write_event()) {
      mgr->mask_del(operation::write);
      events &= ~output_mask;
    }
  }
  if (checkerror && ((revents & error_mask) != 0)) {
    if (revents & POLLNVAL)
      mgr->handle_error(sec::socket_invalid);
    else if (revents & POLLHUP)
      mgr->handle_error(sec::socket_disconnected);
    else
      mgr->handle_error(sec::socket_operation_failed);
    mgr->mask_del(operation::read_write);
    events = 0;
  }
  return events;
}

void multiplexer::add(socket_manager_ptr mgr) {
  CAF_LOG_TRACE(CAF_ARG2("socket", mgr->handle()));
  CAF_ASSERT(index_of(mgr) == -1);
  pollfd new_entry{socket_cast<socket_id>(mgr->handle()),
                   to_bitmask(mgr->mask()), 0};
  pollset_.emplace_back(new_entry);
  managers_.emplace_back(std::move(mgr));
}

void multiplexer::del(ptrdiff_t index) {
  CAF_ASSERT(index != -1);
  pollset_.erase(pollset_.begin() + index);
  managers_.erase(managers_.begin() + index);
}

void multiplexer::write_to_pipe(uint8_t opcode, const socket_manager_ptr& mgr) {
  CAF_ASSERT(opcode == pollset_updater::register_reading_code
             || opcode == pollset_updater::register_writing_code
             || opcode == pollset_updater::init_manager_code
             || opcode == pollset_updater::shutdown_code);
  CAF_ASSERT(mgr != nullptr || opcode == pollset_updater::shutdown_code);
  pollset_updater::msg_buf buf;
  if (opcode != pollset_updater::shutdown_code)
    mgr->ref();
  buf[0] = static_cast<byte>(opcode);
  auto value = reinterpret_cast<intptr_t>(mgr.get());
  memcpy(buf.data() + 1, &value, sizeof(intptr_t));
  ptrdiff_t res = -1;
  { // Lifetime scope of guard.
    std::lock_guard<std::mutex> guard{write_lock_};
    if (write_handle_ != invalid_socket)
      res = write(write_handle_, buf);
  }
  if (res <= 0 && opcode != pollset_updater::shutdown_code)
    mgr->deref();
}

} // namespace caf::net
