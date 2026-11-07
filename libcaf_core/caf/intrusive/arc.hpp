// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/add_ref.hpp"
#include "caf/adopt_ref.hpp"
#include "caf/detail/assert.hpp"
#include "caf/intrusive/atomic_control_block.hpp"

#include <concepts>
#include <utility>

namespace caf::intrusive {

template <class Managed>
class arc_weak_ptr;

/// An atomic reference-counted (ARC) smart pointer. Akin to `std::shared_ptr`,
/// i.e., holds two pointers internally: one to the managed object and one to
/// the control block. The type of the control block is fixed to
/// `atomic_control_block` (or a type derived from it).
template <class Managed>
class arc_ptr {
public:
  template <class>
  friend class arc_ptr;

  template <class>
  friend class arc_weak_ptr;

  /// The type of the managed object.
  using managed_type = Managed;

  constexpr arc_ptr() noexcept = default;

  constexpr arc_ptr(adopt_ref_t, managed_type* managed,
                    atomic_control_block* storage) noexcept
    : managed_(managed), storage_(storage) {
    // nop
  }

  arc_ptr(add_ref_t, managed_type* managed,
          atomic_control_block* storage) noexcept
    : managed_(managed), storage_(storage) {
    if (storage_) {
      CAF_ASSERT(managed_ != nullptr);
      storage_->ref();
    }
  }

  arc_ptr(arc_ptr&& other) noexcept
    : arc_ptr(adopt_ref, std::exchange(other.managed_, nullptr),
              std::exchange(other.storage_, nullptr)) {
    // nop
  }

  template <std::derived_from<Managed> Derived>
  arc_ptr(arc_ptr<Derived>&& other) noexcept
    : arc_ptr(adopt_ref, std::exchange(other.managed_, nullptr),
              std::exchange(other.storage_, nullptr)) {
    // nop
  }

  arc_ptr(const arc_ptr& other) noexcept
    : arc_ptr(add_ref, other.managed_, other.storage_) {
    // nop
  }

  template <std::derived_from<Managed> Derived>
  arc_ptr(const arc_ptr<Derived>& other) noexcept
    : arc_ptr(add_ref, other.managed_, other.storage_) {
    // nop
  }

  arc_ptr& operator=(const arc_ptr& other) noexcept {
    if (this != &other) {
      reset(add_ref, other.managed_, other.storage_);
    }
    return *this;
  }

  arc_ptr& operator=(arc_ptr&& other) noexcept {
    if (this != &other) {
      reset(adopt_ref, std::exchange(other.managed_, nullptr),
            std::exchange(other.storage_, nullptr));
    }
    return *this;
  }

  template <std::derived_from<Managed> Derived>
  arc_ptr& operator=(const arc_ptr<Derived>& other) noexcept {
    reset(add_ref, other.managed_, other.storage_);
    return *this;
  }

  template <std::derived_from<Managed> Derived>
  arc_ptr& operator=(arc_ptr<Derived>&& other) noexcept {
    reset(adopt_ref, std::exchange(other.managed_, nullptr),
          std::exchange(other.storage_, nullptr));
    return *this;
  }

  ~arc_ptr() noexcept {
    if (storage_) {
      storage_->deref();
    }
  }

  /// Checks whether the smart pointer is managing an object.
  constexpr explicit operator bool() const noexcept {
    return managed_ != nullptr;
  }

  /// Returns a weak reference to the managed object.
  arc_weak_ptr<Managed> downgrade() const noexcept;

  /// Accesses the managed object.
  managed_type* operator->() const noexcept {
    CAF_ASSERT(managed_ != nullptr);
    return managed_;
  }

  /// Accesses the managed object.
  managed_type& operator*() const noexcept {
    CAF_ASSERT(managed_ != nullptr);
    return *managed_;
  }

  /// Returns the number of strong references to the managed object.
  size_t strong_count() const noexcept {
    return storage_->strong_reference_count();
  }

  /// Returns the number of weak references to the managed object.
  size_t weak_count() const noexcept {
    return storage_->weak_reference_count();
  }

  /// Returns a pointer for checking whether two references point to the same
  /// object.
  const atomic_control_block* storage() const noexcept {
    return storage_;
  }

private:
  /// Resets the smart pointer to manage a new object.
  template <class Policy>
  void
  reset(Policy, managed_type* managed, atomic_control_block* storage) noexcept {
    if constexpr (std::is_same_v<Policy, add_ref_t>) {
      if (storage) {
        CAF_ASSERT(managed != nullptr);
        storage->ref();
      }
    }
    if (storage_) {
      storage_->deref();
    }
    managed_ = managed;
    storage_ = storage;
  }

  managed_type* managed_ = nullptr;
  atomic_control_block* storage_ = nullptr;
};

} // namespace caf::intrusive

namespace caf::detail {

template <class Managed>
struct default_arc_storage final : public intrusive::atomic_control_block {
public:
  template <class... Args>
  explicit default_arc_storage(std::in_place_t, Args&&... args) {
    new (&managed_) Managed(std::forward<Args>(args)...);
  }

  ~default_arc_storage() noexcept override {
    // nop
  }

  void delete_this() noexcept override {
    delete this;
  }

  void destroy_managed() noexcept override {
    managed_.~Managed();
  }

  Managed* managed() noexcept {
    return &managed_;
  }

private:
  union {
    Managed managed_;
  };
};

} // namespace caf::detail

namespace caf::intrusive {

/// Creates a new instance of type `Managed` and returns an `arc_ptr` managing
/// it. Control block and managed object are allocated together.
template <class Managed, class... Args>
arc_ptr<Managed> make_arc(Args&&... args) {
  using storage_t = caf::detail::default_arc_storage<Managed>;
  auto* ptr = new storage_t(std::in_place, std::forward<Args>(args)...);
  return {adopt_ref, ptr->managed(), ptr};
}

/// The weak counterpart to `arc_ptr` that holds a non-owning reference to the
/// managed object. Can be upgraded to a strong reference.
/// @note does not support constructing from a derived `Managed` type, because
///       converting pointers to a virtual base type requires accessing the
///       object (which may have been destroyed already).
template <class Managed>
class arc_weak_ptr {
public:
  template <class>
  friend class arc_ptr;

  template <class>
  friend class arc_weak_ptr;

  /// The type of the managed object.
  using managed_type = Managed;

  constexpr arc_weak_ptr() noexcept = default;

  constexpr arc_weak_ptr(adopt_ref_t, managed_type* managed,
                         atomic_control_block* storage) noexcept
    : managed_(managed), storage_(storage) {
    // nop
  }

  arc_weak_ptr(add_ref_t, managed_type* managed,
               atomic_control_block* storage) noexcept
    : managed_(managed), storage_(storage) {
    if (storage_) {
      CAF_ASSERT(managed_ != nullptr);
      storage_->ref_weak();
    }
  }

  arc_weak_ptr(const arc_weak_ptr& other) noexcept
    : arc_weak_ptr(add_ref, other.managed_, other.storage_) {
    // nop
  }

  arc_weak_ptr(arc_weak_ptr&& other) noexcept
    : arc_weak_ptr(adopt_ref, std::exchange(other.managed_, nullptr),
                   std::exchange(other.storage_, nullptr)) {
    // nop
  }

  arc_weak_ptr& operator=(const arc_weak_ptr& other) noexcept {
    if (this != &other) {
      reset(add_ref, other.managed_, other.storage_);
    }
    return *this;
  }

  arc_weak_ptr& operator=(arc_weak_ptr&& other) noexcept {
    if (this != &other) {
      reset(adopt_ref, std::exchange(other.managed_, nullptr),
            std::exchange(other.storage_, nullptr));
    }
    return *this;
  }

  ~arc_weak_ptr() noexcept {
    if (storage_) {
      storage_->deref_weak();
    }
  }

  constexpr explicit operator bool() const noexcept {
    return managed_ != nullptr;
  }

  /// Checks whether the managed object was already deleted.
  bool expired() const noexcept {
    return !storage_ || storage_->strong_reference_count() == 0;
  }

  /// Returns the number of strong references to the managed object.
  size_t strong_count() const noexcept {
    CAF_ASSERT(storage_ != nullptr);
    return storage_->strong_reference_count();
  }

  /// Returns the number of weak references to the managed object.
  size_t weak_count() const noexcept {
    CAF_ASSERT(storage_ != nullptr);
    return storage_->weak_reference_count();
  }

  /// Returns a pointer for checking whether two references point to the same
  /// object.
  const atomic_control_block* storage() const noexcept {
    return storage_;
  }

  /// Tries to promote this weak reference to a strong reference. Returns a
  /// null pointer if the managed object was already destroyed.
  arc_ptr<Managed> upgrade() const noexcept {
    if (storage_ && storage_->upgrade_weak()) {
      return {adopt_ref, managed_, storage_};
    }
    return {};
  }

private:
  /// Resets the smart pointer to manage a new object.
  template <class Policy>
  void
  reset(Policy, managed_type* managed, atomic_control_block* storage) noexcept {
    if constexpr (std::is_same_v<Policy, add_ref_t>) {
      if (storage) {
        CAF_ASSERT(managed != nullptr);
        storage->ref_weak();
      }
    }
    if (storage_) {
      storage_->deref_weak();
    }
    managed_ = managed;
    storage_ = storage;
  }

  managed_type* managed_ = nullptr;
  atomic_control_block* storage_ = nullptr;
};

template <class Managed>
arc_weak_ptr<Managed> arc_ptr<Managed>::downgrade() const noexcept {
  return {add_ref, managed_, storage_};
}

} // namespace caf::intrusive
