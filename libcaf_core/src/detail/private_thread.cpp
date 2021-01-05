// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/private_thread.hpp"

#include "caf/actor_system.hpp"
#include "caf/config.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/logger.hpp"
#include "caf/resumable.hpp"
#include "caf/scoped_execution_unit.hpp"

namespace caf::detail {

void private_thread::run(actor_system* sys) {
  CAF_LOG_TRACE("");
  scoped_execution_unit ctx{sys};
  auto resume = [&ctx](resumable* job) {
    auto res = job->resume(&ctx, std::numeric_limits<size_t>::max());
    while (res == resumable::resume_later)
      res = job->resume(&ctx, std::numeric_limits<size_t>::max());
    return res;
  };
  for (;;) {
    auto [job, done] = await();
    if (job) {
      CAF_ASSERT(job->subtype() != resumable::io_actor);
      resume(job);
      intrusive_ptr_release(job);
    }
    if (done)
      return;
  }
}

void private_thread::resume(resumable* ptr) {
  std::unique_lock<std::mutex> guard{mtx_};
  CAF_ASSERT(job_ == nullptr);
  job_ = ptr;
  cv_.notify_all();
}

bool private_thread::stop() {
  {
    std::unique_lock<std::mutex> guard{mtx_};
    shutdown_ = true;
    cv_.notify_all();
  }
  thread_.join();
  return true;
}

std::pair<resumable*, bool> private_thread::await() {
  std::unique_lock<std::mutex> guard(mtx_);
  while (job_ == nullptr && !shutdown_)
    cv_.wait(guard);
  auto ptr = job_;
  if (ptr)
    job_ = nullptr;
  return {ptr, shutdown_};
}

private_thread* private_thread::launch(actor_system* sys) {
  auto ptr = std::make_unique<private_thread>();
  ptr->thread_ = std::thread{exec, sys, ptr.get()};
  return ptr.release();
}

void private_thread::exec(actor_system* sys, private_thread* this_ptr) {
  CAF_SET_LOGGER_SYS(sys);
  detail::set_thread_name("caf.thread");
  sys->thread_started();
  this_ptr->run(sys);
  sys->thread_terminates();
}

} // namespace caf::detail
