/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/private_thread.hpp"

#include "caf/config.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/logger.hpp"
#include "caf/scheduled_actor.hpp"

namespace caf {
namespace detail {

private_thread::private_thread(scheduled_actor* self)
    : self_destroyed_(false),
      self_(self),
      state_(active),
      system_(self->system()) {
  intrusive_ptr_add_ref(self->ctrl());
  system_.inc_detached_threads();
}

void private_thread::run() {
  auto job = self_.load();
  CAF_ASSERT(job != nullptr);
  CAF_SET_LOGGER_SYS(&job->system());
  CAF_PUSH_AID(job->id());
  CAF_LOG_TRACE("");
  scoped_execution_unit ctx{&job->system()};
  auto max_throughput = std::numeric_limits<size_t>::max();
  bool resume_later;
  for (;;) {
    state_ = await_resume_or_shutdown;
    do {
      resume_later = false;
      switch (job->resume(&ctx, max_throughput)) {
        case resumable::resume_later:
          resume_later = true;
          break;
        case resumable::done:
          intrusive_ptr_release(job->ctrl());
          return;
        case resumable::awaiting_message:
          intrusive_ptr_release(job->ctrl());
          break;
        case resumable::shutdown_execution_unit:
          return;
      }
    } while (resume_later);
    // wait until actor becomes ready again or was destroyed
    if (!await_resume())
      return;
  }
}

bool private_thread::await_resume() {
  std::unique_lock<std::mutex> guard(mtx_);
  while (state_ == await_resume_or_shutdown)
    cv_.wait(guard);
  return state_ == active;
}

void private_thread::resume() {
  std::unique_lock<std::mutex> guard(mtx_);
  state_ = active;
  cv_.notify_one();
}

void private_thread::shutdown() {
  std::unique_lock<std::mutex> guard(mtx_);
  state_ = shutdown_requested;
  cv_.notify_one();
}

void private_thread::exec(private_thread* this_ptr) {
  detail::set_thread_name("caf.actor");
  this_ptr->system_.thread_started();
  this_ptr->run();
  // make sure to not destroy the private thread object before the
  // detached actor is destroyed and this object is unreachable
  this_ptr->await_self_destroyed();
  // signalize destruction of detached thread to registry
  this_ptr->system_.thread_terminates();
  this_ptr->system_.dec_detached_threads();
  // done
  delete this_ptr;
}

void private_thread::notify_self_destroyed() {
  std::unique_lock<std::mutex> guard(mtx_);
  self_destroyed_ = true;
  cv_.notify_one();
}

void private_thread::await_self_destroyed() {
  std::unique_lock<std::mutex> guard(mtx_);
  while (!self_destroyed_)
    cv_.wait(guard);
}

void private_thread::start() {
  std::thread{exec, this}.detach();
}

} // namespace detail
} // namespace caf
