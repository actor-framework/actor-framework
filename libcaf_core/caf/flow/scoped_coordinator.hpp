// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <condition_variable>
#include <mutex>

#include "caf/flow/coordinator.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"

namespace caf::flow {

class CAF_CORE_EXPORT scoped_coordinator final : public ref_counted,
                                                 public coordinator {
public:
  void run();

  void ref_coordinator() const noexcept override;

  void deref_coordinator() const noexcept override;

  void schedule(action what) override;

  void post_internally(action what) override;

  void watch(disposable what) override;

  friend void intrusive_ptr_add_ref(const scoped_coordinator* ptr) {
    ptr->ref();
  }

  friend void intrusive_ptr_release(scoped_coordinator* ptr) {
    ptr->deref();
  }

  static intrusive_ptr<scoped_coordinator> make();

private:
  scoped_coordinator() = default;

  void drop_disposed_flows();

  std::vector<disposable> watched_disposables_;

  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::vector<action> actions_;
};

using scoped_coordinator_ptr = intrusive_ptr<scoped_coordinator>;

inline scoped_coordinator_ptr make_scoped_coordinator() {
  return scoped_coordinator::make();
}

} // namespace caf::flow
