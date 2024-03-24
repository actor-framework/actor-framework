// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/coordinated.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"
#include "caf/timespan.hpp"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

namespace caf::flow {

class CAF_CORE_EXPORT scoped_coordinator final : public ref_counted,
                                                 public coordinator {
public:
  // -- factories --------------------------------------------------------------

  static intrusive_ptr<scoped_coordinator> make();

  // -- execution --------------------------------------------------------------

  void run();

  size_t run_some();

  template <class Rep, class Period>
  size_t run_some(std::chrono::duration<Rep, Period> relative_timeout) {
    return run_some(steady_time() + relative_timeout);
  }

  size_t run_some(steady_time_point timeout);

  // -- reference counting -----------------------------------------------------

  void ref_execution_context() const noexcept override;

  void deref_execution_context() const noexcept override;

  friend void intrusive_ptr_add_ref(const scoped_coordinator* ptr) {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const scoped_coordinator* ptr) {
    ptr->deref();
  }

  // -- properties -------------------------------------------------------------

  /// Returns the number of pending (delayed and scheduled) actions.
  [[nodiscard]] size_t pending_actions() const noexcept;

  // -- lifetime management ----------------------------------------------------

  void release_later(coordinated_ptr& child) override;

  void watch(disposable what) override;

  size_t watched_disposables_count() const noexcept {
    return watched_disposables_.size();
  }

  // -- time -------------------------------------------------------------------

  steady_time_point steady_time() override;

  // -- scheduling of actions --------------------------------------------------

  void schedule(action what) override;

  void delay(action what) override;

  disposable delay_until(steady_time_point abs_time, action what) override;

private:
  scoped_coordinator() = default;

  action next(bool blocking);

  action next(steady_time_point timeout);

  void drop_disposed_flows();

  /// Stores objects that need to be disposed before returning from `run`.
  std::vector<disposable> watched_disposables_;

  /// Stores children that were marked for release while running an action.
  std::vector<coordinated_ptr> released_;

  /// Stores delayed actions until they are due.
  std::multimap<steady_time_point, action> delayed_;

  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::deque<action> actions_;
};

using scoped_coordinator_ptr = intrusive_ptr<scoped_coordinator>;

inline scoped_coordinator_ptr make_scoped_coordinator() {
  return scoped_coordinator::make();
}

} // namespace caf::flow
