/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_METAINFO_HPP
#define CAF_METAINFO_HPP

#include <atomic>
#include <vector>
#include <algorithm>
#include <functional>

#include "caf/all.hpp"
#include "caf/config.hpp"
#include "caf/optional.hpp"

#include "caf/opencl/device.hpp"
#include "caf/opencl/global.hpp"
#include "caf/opencl/program.hpp"
#include "caf/opencl/platform.hpp"
#include "caf/opencl/smart_ptr.hpp"
#include "caf/opencl/actor_facade.hpp"

#include "caf/detail/singletons.hpp"

namespace caf {
namespace opencl {

class metainfo : public detail::abstract_singleton {

  friend class program;
  friend class detail::singletons;
  friend command_queue_ptr get_command_queue(uint32_t id);

public:
  /// Get a list of all available devices. This is depricated, use the more specific
  /// get_deivce and get_deivce_if functions.
  /// (Returns only devices of the first discovered platform).
  const std::vector<device>& get_devices() const CAF_DEPRECATED;
  /// Get the device with id. These ids are assigned sequientally to all available devices.
  const optional<const device&> get_device(size_t id = 0) const;
  /// Get the first device that satisfies the predicate.
  /// The predicate should accept a `const device&` and return a bool;
  template <class UnaryPredicate>
  const optional<const device&> get_device_if(UnaryPredicate p) const {
    for (auto& pl : platforms_) {
      for (auto& dev : pl.get_devices()) {
        if (p(dev))
          return dev;
      }
    }
    return none;
  }

  /// Get metainfo instance.
  static metainfo* instance();

private:
  metainfo() = default;

  void stop() override;
  void initialize() override;
  void dispose() override;

  std::vector<platform> platforms_;
};

} // namespace opencl
} // namespace caf

#endif // CAF_METAINFO_HPP
