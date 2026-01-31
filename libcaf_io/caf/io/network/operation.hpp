// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/caf_deprecated.hpp"
#include "caf/detail/io_export.hpp"

#include <string>

namespace caf::io::network {

/// Identifies network IO operations, i.e., read or write.
enum class operation {
  read,
  write,
  propagate_error,
};

CAF_DEPRECATED("use caf.net instead")
CAF_IO_EXPORT std::string to_string(operation);

CAF_DEPRECATED("use caf.net instead")
CAF_IO_EXPORT bool from_string(std::string_view, operation&);

CAF_DEPRECATED("use caf.net instead")
CAF_IO_EXPORT bool from_integer(std::underlying_type_t<operation>, operation&);

template <class Inspector>
CAF_DEPRECATED("use caf.net instead") bool inspect(Inspector& f, operation& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::io::network
