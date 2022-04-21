// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::tag {

/// Tells message-oriented transports to *not* implicitly register an
/// application for reading as part of the initialization when used a base type.
struct no_auto_reading {};

} // namespace caf::tag
