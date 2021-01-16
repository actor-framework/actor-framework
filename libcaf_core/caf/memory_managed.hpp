// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

namespace caf {

class CAF_CORE_EXPORT [[deprecated]] memory_managed {
public:
  virtual void request_deletion(bool decremented_rc) const noexcept;

protected:
  virtual ~memory_managed();
};

} // namespace caf
