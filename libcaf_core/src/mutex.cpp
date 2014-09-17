
#include "caf/mutex.hpp"

namespace caf {

// const defer_lock_t defer_lock = {};
// const try_to_lock_t try_to_lock = {};
// const adopt_lock_t adopt_lock = {};

mutex::~mutex() {
  // nop
}

void mutex::lock() {
  mutex_lock(&m_mtx);
}

bool mutex::try_lock() noexcept {
  return (1 == mutex_trylock(&m_mtx));
}

void mutex::unlock() noexcept {
  mutex_unlock(&m_mtx);
}

} // namespace caf
