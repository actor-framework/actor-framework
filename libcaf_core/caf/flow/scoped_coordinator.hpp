// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <condition_variable>
#include <map>
#include <mutex>

#include "caf/flow/coordinator.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"

namespace caf::flow {

class CAF_CORE_EXPORT scoped_coordinator final : public ref_counted,
                                                 public coordinator {
public:
  // -- factories --------------------------------------------------------------

  static intrusive_ptr<scoped_coordinator> make();

  // -- execution --------------------------------------------------------------

  void run();

  size_t run_some();

  // -- reference counting -----------------------------------------------------

  void ref_execution_context() const noexcept override;

  void deref_execution_context() const noexcept override;

  friend void intrusive_ptr_add_ref(const scoped_coordinator* ptr) {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const scoped_coordinator* ptr) {
    ptr->deref();
  }

  // -- lifetime management ----------------------------------------------------

  void watch(disposable what) override;

  // -- time -------------------------------------------------------------------

  steady_time_point steady_time() override;

  // -- scheduling of actions --------------------------------------------------

  void schedule(action what) override;

  void delay(action what) override;

  disposable delay_until(steady_time_point abs_time, action what) override;

private:
  scoped_coordinator() = default;

  action next(bool blocking);

  void drop_disposed_flows();

  std::vector<disposable> watched_disposables_;

  std::multimap<steady_time_point, action> delayed_;

  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::vector<action> actions_;
};

using scoped_coordinator_ptr = intrusive_ptr<scoped_coordinator>;

inline scoped_coordinator_ptr make_scoped_coordinator() {
  return scoped_coordinator::make();
}

} // namespace caf::flow
