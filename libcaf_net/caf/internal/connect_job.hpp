// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"

#include "caf/detail/atomic_ref_count.hpp"
#include "caf/fwd.hpp"
#include "caf/internal/async_client_config_base.hpp"
#include "caf/resumable.hpp"

#include <cstddef>
#include <cstdint>

namespace caf::internal {

class connect_job : public resumable {
public:
  explicit connect_job(const_async_client_config_base_ptr config)
    : config_(std::move(config)) {
    // nop
  }

  void ref() const noexcept override;

  void deref() const noexcept override;

  void resume(scheduler*, uint64_t) override;

protected:
  virtual bool canceled() const noexcept = 0;

  virtual void fail(error reason) = 0;

  virtual void handover(net::stream_socket fd) = 0;

  virtual void handover(net::ssl::connection conn) = 0;

  const_async_client_config_base_ptr config_;

private:
  mutable detail::atomic_ref_count ref_count_;

  size_t attempts_ = 0;
};

} // namespace caf::internal
