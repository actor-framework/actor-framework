#ifndef NO_RESUME_HPP
#define NO_RESUME_HPP

#include <chrono>
#include <utility>

#include "caf/exception.hpp"
#include "caf/exit_reason.hpp"
#include "caf/detail/logging.hpp"
#include "caf/policy/resume_policy.hpp"

namespace caf {
namespace policy {

class no_resume {
 public:
  template <class Base, class Derived>
  struct mixin : Base {
    template <class... Ts>
    mixin(Ts&&... args) : Base(std::forward<Ts>(args)...) {
      // nop
    }

    void attach_to_scheduler() {
      this->ref();
    }

    void detach_from_scheduler() {
      this->deref();
    }

    resumable::resume_result resume(execution_unit*, size_t) {
      CAF_LOG_TRACE("");
      uint32_t rsn = exit_reason::normal;
      std::exception_ptr eptr = nullptr;
      try {
        this->act();
      }
      catch (actor_exited& e) {
        rsn = e.reason();
      }
      catch (...) {
        rsn = exit_reason::unhandled_exception;
        eptr = std::current_exception();
      }
      if (eptr) {
        auto opt_reason = this->handle(eptr);
        rsn = *opt_reason;
      }
      this->planned_exit_reason(rsn);
      try {
        this->on_exit();
      }
      catch (...) {
        // simply ignore exception
      }
      // exit reason might have been changed by on_exit()
      this->cleanup(this->planned_exit_reason());
      return resumable::done;
    }
  };

  template <class Actor>
  void await_ready(Actor* self) {
    self->await_data();
  }
};

} // namespace policy
} // namespace caf

#endif // NO_RESUME_HPP
