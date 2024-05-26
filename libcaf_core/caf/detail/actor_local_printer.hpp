// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"

#include <string_view>

namespace caf::detail {

class CAF_CORE_EXPORT actor_local_printer : public ref_counted {
public:
  ~actor_local_printer() override;

  virtual void write(std::string&& arg) = 0;

  virtual void write(const char* arg) = 0;

  virtual void flush() = 0;
};

using actor_local_printer_ptr = intrusive_ptr<actor_local_printer>;

} // namespace caf::detail
