#ifndef RECEIVE_POLICY_H
#define RECEIVE_POLICY_H

#include <cstddef>
#include <utility>

#include "caf/config.hpp"

namespace caf {
namespace io {

enum class receive_policy_flag {
  at_least,
  at_most,
  exactly
};

class receive_policy {

  receive_policy() = delete;

public:

  using config = std::pair<receive_policy_flag, size_t>;

  static inline config at_least(size_t num_bytes) {
    CAF_ASSERT(num_bytes > 0);
    return {receive_policy_flag::at_least, num_bytes};
  }

  static inline config at_most(size_t num_bytes) {
    CAF_ASSERT(num_bytes > 0);
    return {receive_policy_flag::at_most, num_bytes};
  }

  static inline config exactly(size_t num_bytes) {
    CAF_ASSERT(num_bytes > 0);
    return {receive_policy_flag::exactly, num_bytes};
  }

};

} // namespace io
} // namespace caf

#endif // RECEIVE_POLICY_H
