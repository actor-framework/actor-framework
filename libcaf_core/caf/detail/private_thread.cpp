// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/private_thread.hpp"

#include "caf/actor_system.hpp"
#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/log/core.hpp"
#include "caf/resumable.hpp"
#include "caf/thread_owner.hpp"

namespace caf::detail {

void private_thread::run(actor_system* sys) {
  auto lg = log::core::trace("");
  auto resume = [&sys](resumable* job) {
    auto res = job->resume(&sys->scheduler(),
                           std::numeric_limits<size_t>::max());
    while (res == resumable::resume_later)
      res = job->resume(&sys->scheduler(), std::numeric_limits<size_t>::max());
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
  auto raw_ptr = ptr.get();
  ptr->thread_ = sys->launch_thread("caf.thread", thread_owner::pool,
                                    [raw_ptr, sys] { raw_ptr->run(sys); });
  return ptr.release();
}

} // namespace caf::detail
