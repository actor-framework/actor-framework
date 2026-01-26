// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/async/consumer.hpp"
#include "caf/async/policy.hpp"
#include "caf/async/producer.hpp"
#include "caf/callback.hpp"
#include "caf/config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/assert.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/make_counted.hpp"
#include "caf/raise_error.hpp"
#include "caf/ref_counted.hpp"
#include "caf/resumable.hpp"
#include "caf/sec.hpp"

#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <span>

namespace caf::async {

/// A Single Producer Single Consumer buffer. The buffer uses a "soft bound",
/// which means that the producer announces a desired maximum for in-flight
/// items that the buffer uses for its bookkeeping, but the producer may add
/// more than that number of items. Allowing producers to go "beyond the limit"
/// is intended for producer that transform inputs into outputs where one input
/// event can produce multiple output items.
///
/// Aside from providing storage, this buffer also resumes the consumer if data
/// is available and signals demand to the producer whenever the consumer takes
/// data out of the buffer.
template <class T>
class spsc_buffer : public ref_counted {
public:
  using value_type = T;

  using lock_type = std::unique_lock<std::mutex>;

  /// Packs various status flags for the buffer into a single struct.
  struct flags {
    /// Stores whether `close` has been called.
    bool closed : 1;
    /// Stores whether `cancel` has been called.
    bool canceled : 1;
  };

  /// @param capacity The maximum number of items the buffer can hold. Treated
  ///                 as a "soft limit" by `push`, i.e., the buffer may
  ///                 temporarily hold more items than the capacity unless the
  ///                 producer only calls `try_push`, which treats the capacity
  ///                 as a hard limit.
  /// @param min_pull_size The minimum number of items the consumer must pull
  ///                      before the producer is signaled to produce more.
  spsc_buffer(size_t capacity, size_t min_pull_size)
    : capacity_(capacity), min_pull_size_(min_pull_size) {
    memset(&flags_, 0, sizeof(flags));
    // Allocate some extra space in the buffer in case the producer goes beyond
    // the announced capacity.
    buf_.reserve(capacity + (capacity / 2));
    // Note: this buffer can never go above its limit since it's a short-term
    // buffer for the consumer that cannot ask for more than capacity
    // items.
    consumer_buf_.reserve(capacity);
  }

  /// Appends to the buffer and calls `on_producer_wakeup` on the consumer if
  /// the buffer becomes non-empty.
  /// @returns the remaining capacity after inserting the items.
  /// @note Items are always copied into the buffer, even after reaching the
  ///       capacity. This allows the buffer to absorb small bursts of items
  ///       without forcing external buffering.
  size_t push(std::span<const T> items) {
    lock_type guard{mtx_};
    CAF_ASSERT(producer_ != nullptr);
    CAF_ASSERT(!flags_.closed);
    buf_.insert(buf_.end(), items.begin(), items.end());
    if (buf_.size() == items.size() && consumer_)
      consumer_->on_producer_wakeup();
    if (capacity_ > buf_.size())
      return capacity_ - buf_.size();
    else
      return 0;
  }

  /// Appends to the buffer and calls `on_producer_wakeup` on the consumer if
  /// the buffer becomes non-empty.
  /// @returns the remaining capacity after inserting the items.
  /// @note Items are always copied into the buffer, even after reaching the
  ///       capacity. This allows the buffer to absorb small bursts of items
  ///       without forcing external buffering.
  size_t push(const T& item) {
    return push(std::span{&item, 1});
  }

  /// Tries to append an item to the buffer. Unlike `push`, this function
  /// respects the capacity as a hard limit and refuses to insert items if the
  /// buffer is at or above capacity.
  /// @returns `true` if the item was inserted, `false` otherwise.
  bool try_push(const T& item) {
    lock_type guard{mtx_};
    CAF_ASSERT(producer_ != nullptr);
    CAF_ASSERT(!flags_.closed);
    if (buf_.size() >= capacity_)
      return false;
    buf_.push_back(item);
    if (buf_.size() == 1 && consumer_)
      consumer_->on_producer_wakeup();
    return true;
  }

  /// Consumes up to `demand` items from the buffer.
  /// @tparam Policy Either `instant_error_t`, `delay_error_t` or
  ///                `ignore_errors_t`.
  /// @returns a tuple indicating whether the consumer may call pull again and
  ///          how many items were consumed. When returning `false` for the
  ///          first tuple element, the function has called `on_complete` or
  ///          `on_error` on the observer.
  template <class Policy, class Observer>
  std::pair<bool, size_t> pull(Policy policy, size_t demand, Observer& dst) {
    lock_type guard{mtx_};
    return pull_unsafe(guard, policy, demand, dst);
  }

  /// Checks whether there is any pending data in the buffer.
  bool has_data() const noexcept {
    lock_type guard{mtx_};
    return !buf_.empty();
  }

  /// Checks whether the there is data available or whether the producer has
  /// closed or aborted the flow.
  bool has_consumer_event() const noexcept {
    lock_type guard{mtx_};
    return !buf_.empty() || flags_.closed;
  }

  /// Returns how many items are currently available. This may be greater than
  /// the `capacity`.
  size_t available() const noexcept {
    lock_type guard{mtx_};
    return buf_.size();
  }

  /// Returns the error from the producer or a default-constructed error if
  /// abort was not called yet.
  error abort_reason() const {
    lock_type guard{mtx_};
    return err_;
  }

  /// Closes the buffer by request of the producer.
  void close() {
    abort(error{});
  }

  /// Closes the buffer by request of the producer and signals an error to the
  /// consumer.
  void abort(error reason) {
    lock_type guard{mtx_};
    if (!flags_.closed) {
      flags_.closed = true;
      err_ = std::move(reason);
      producer_ = nullptr;
      if (buf_.empty() && consumer_)
        consumer_->on_producer_wakeup();
    }
  }

  /// Closes the buffer by request of the consumer.
  void cancel() {
    lock_type guard{mtx_};
    if (!flags_.canceled) {
      flags_.canceled = true;
      consumer_ = nullptr;
      if (producer_)
        producer_->on_consumer_cancel();
    }
  }

  /// Consumer callback for the initial handshake between producer and consumer.
  void set_consumer(consumer_ptr consumer) {
    CAF_ASSERT(consumer != nullptr);
    lock_type guard{mtx_};
    if (consumer_)
      CAF_RAISE_ERROR(std::logic_error, "SPSC buffer already has a consumer");
    consumer_ = std::move(consumer);
    if (producer_)
      ready();
    else if (flags_.closed)
      consumer_->on_producer_wakeup();
  }

  /// Producer callback for the initial handshake between producer and consumer.
  void set_producer(producer_ptr producer) {
    CAF_ASSERT(producer != nullptr);
    lock_type guard{mtx_};
    if (producer_)
      CAF_RAISE_ERROR(std::logic_error, "SPSC buffer already has a producer");
    producer_ = std::move(producer);
    if (consumer_)
      ready();
    else if (flags_.canceled)
      producer_->on_consumer_cancel();
  }

  /// Returns the capacity as passed to the constructor of the buffer.
  size_t capacity() const noexcept {
    return capacity_;
  }

  // -- unsafe interface for manual locking ------------------------------------

  /// Returns the mutex for this object.
  auto& mtx() const noexcept {
    return mtx_;
  }

  /// Returns how many items are currently available.
  /// @pre 'mtx()' is locked.
  size_t available_unsafe() const noexcept {
    return buf_.size();
  }

  /// Returns the error from the producer.
  /// @pre 'mtx()' is locked.
  const error& abort_reason_unsafe() const noexcept {
    return err_;
  }

  /// Blocks until there is at least one item available or the producer stopped.
  /// @pre the consumer calls `cv.notify_all()` in its `on_producer_wakeup`
  void await_consumer_ready(lock_type& guard, std::condition_variable& cv) {
    while (!flags_.closed && buf_.empty()) {
      cv.wait(guard);
    }
  }

  /// Blocks until there is at least one item available, the producer stopped,
  /// or a timeout occurs.
  /// @pre the consumer calls `cv.notify_all()` in its `on_producer_wakeup`
  template <class TimePoint>
  bool await_consumer_ready(lock_type& guard, std::condition_variable& cv,
                            TimePoint timeout) {
    while (!flags_.closed && buf_.empty())
      if (cv.wait_until(guard, timeout) == std::cv_status::timeout)
        return false;
    return true;
  }

  template <class Policy, class Observer>
  std::pair<bool, size_t>
  pull_unsafe(lock_type& guard, Policy, size_t demand, Observer& dst) {
    CAF_ASSERT(consumer_ != nullptr);
    CAF_ASSERT(consumer_buf_.empty());
    if constexpr (std::is_same_v<Policy, prioritize_errors_t>) {
      if (err_.valid()) {
        consumer_ = nullptr;
        dst.on_error(err_);
        return {false, 0};
      }
    }
    // We must not signal demand to the producer when reading excess elements
    // from the buffer. Otherwise, we end up generating more demand than
    // capacity_ allows us to.
    auto overflow = buf_.size() <= capacity_ ? 0u : buf_.size() - capacity_;
    auto next_n = [this, &demand] { return std::min(demand, buf_.size()); };
    size_t consumed = 0;
    for (auto n = next_n(); n > 0; n = next_n()) {
      using std::make_move_iterator;
      consumer_buf_.assign(make_move_iterator(buf_.begin()),
                           make_move_iterator(buf_.begin() + n));
      buf_.erase(buf_.begin(), buf_.begin() + n);
      if (n > overflow) {
        signal_demand(n - overflow);
      }
      guard.unlock();
      auto items = std::span<const T>{consumer_buf_.data(), n};
      for (auto& item : items)
        dst.on_next(item);
      demand -= n;
      consumed += n;
      consumer_buf_.clear();
      guard.lock();
      overflow = buf_.size() <= capacity_ ? 0u : buf_.size() - capacity_;
    }
    if (!buf_.empty() || !flags_.closed) {
      return {true, consumed};
    }
    consumer_ = nullptr;
    if (err_.empty())
      dst.on_complete();
    else
      dst.on_error(err_);
    return {false, consumed};
  }

private:
  void ready() {
    producer_->on_consumer_ready();
    consumer_->on_producer_ready();
    if (!buf_.empty()) {
      consumer_->on_producer_wakeup();
      if (capacity_ > buf_.size())
        signal_demand(capacity_ - buf_.size());
    } else {
      signal_demand(capacity_);
    }
  }

  void signal_demand(size_t new_demand) {
    demand_ += new_demand;
    if (demand_ >= min_pull_size_ && producer_) {
      producer_->on_consumer_demand(demand_);
      demand_ = 0;
    }
  }

  /// Guards access to all other member variables.
  mutable std::mutex mtx_;

  /// Caches in-flight items.
  std::vector<T> buf_;

  /// Stores how many items the buffer may hold at any time.
  size_t capacity_;

  /// Configures the minimum amount of free buffer slots that we signal to the
  /// producer.
  size_t min_pull_size_;

  /// Demand that has not yet been signaled back to the producer.
  size_t demand_ = 0;

  /// Stores whether `close` has been called.
  flags flags_;

  /// Stores the abort reason.
  error err_;

  /// Callback handle to the consumer.
  consumer_ptr consumer_;

  /// Callback handle to the producer.
  producer_ptr producer_;

  /// Caches items before passing them to the consumer (without lock).
  std::vector<T> consumer_buf_;
};

/// @relates spsc_buffer
template <class T>
using spsc_buffer_ptr = intrusive_ptr<spsc_buffer<T>>;

/// @relates spsc_buffer
template <class T, bool IsProducer>
struct resource_ctrl : ref_counted {
  using buffer_ptr = spsc_buffer_ptr<T>;

  explicit resource_ctrl(buffer_ptr ptr) : buf(std::move(ptr)) {
    // nop
  }

  ~resource_ctrl() override {
    if (buf) {
      if constexpr (IsProducer) {
        buf->abort(make_error(sec::resource_destroyed));
      } else {
        buf->cancel();
      }
    }
  }

  buffer_ptr try_open() {
    auto res = buffer_ptr{};
    std::unique_lock guard{mtx};
    if (buf) {
      res.swap(buf);
    }
    return res;
  }

  mutable std::mutex mtx;
  buffer_ptr buf;
};

/// Consumes data from an `spsc_buffer`.
template <class T>
class spsc_buffer_consumer : public ref_counted,
                             public action::impl,
                             public consumer {
public:
  using on_wakeup_signature = void(spsc_buffer_consumer<T>&);

  template <class OnWakeup>
  spsc_buffer_consumer(caf::strong_actor_ptr owner, spsc_buffer_ptr<T> buf,
                       OnWakeup on_wakeup)
    : owner_(std::move(owner)), buf_(std::move(buf)) {
    using impl_t = callback_impl<OnWakeup, on_wakeup_signature>;
    on_wakeup_ = std::make_shared<impl_t>(std::move(on_wakeup));
  }

  ~spsc_buffer_consumer() override {
    if (buf_) {
      buf_->cancel();
    }
  }

  template <class Observer>
  std::pair<bool, size_t> pull(size_t demand, Observer& dst) {
    spsc_buffer_ptr<T> buf;
    {
      std::unique_lock guard{mtx_};
      buf = buf_;
    }
    if (!buf) {
      return {false, 0};
    }
    auto [again, pulled] = buf->pull(delay_errors_t{}, demand, dst);
    if (!again) {
      dispose();
    }
    return {again, pulled};
  }

  bool disposed() const noexcept override {
    std::unique_lock guard{mtx_};
    return !owner_;
  }

  void dispose() override {
    buffer_ptr_t buf;
    {
      std::unique_lock guard{mtx_};
      owner_ = nullptr;
      buf_.swap(buf);
      on_wakeup_ = nullptr;
    }
    if (buf) {
      buf->cancel();
    }
  }

  void on_producer_ready() override {
    // nop
  }

  void on_producer_wakeup() override {
    std::unique_lock guard{mtx_};
    if (owner_) {
      owner_->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           action{this}),
                      nullptr);
    }
  }

  void ref_consumer() const noexcept override {
    ref();
  }

  void deref_consumer() const noexcept override {
    deref();
  }

  action::state current_state() const noexcept override {
    std::unique_lock guard{mtx_};
    return owner_ ? action::state::scheduled : action::state::disposed;
  }

  void resume(scheduler*, uint64_t) override {
    on_wakeup_t on_wakeup;
    {
      std::unique_lock guard{mtx_};
      on_wakeup = on_wakeup_;
    }
    if (on_wakeup) {
      (*on_wakeup)(*this);
    }
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  friend void intrusive_ptr_add_ref(const spsc_buffer_consumer* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const spsc_buffer_consumer* ptr) noexcept {
    ptr->deref();
  }

private:
  using buffer_ptr_t = spsc_buffer_ptr<T>;

  using on_wakeup_t = shared_callback_ptr<on_wakeup_signature>;

  /// Guards access to the state of the consumer.
  mutable std::mutex mtx_;

  /// The actor that owns this consumer.
  caf::strong_actor_ptr owner_;

  /// The decorated buffer.
  buffer_ptr_t buf_;

  /// Callback handle to the consumer.
  on_wakeup_t on_wakeup_;
};

/// @relates spsc_buffer_consumer
template <class T>
using spsc_buffer_consumer_ptr = intrusive_ptr<spsc_buffer_consumer<T>>;

/// Grants read access to the first consumer that calls `open` on the resource.
/// Cancels consumption of items on the buffer if the resources gets destroyed
/// before opening it.
/// @relates spsc_buffer
template <class T>
class consumer_resource {
public:
  using value_type = T;

  using buffer_type = spsc_buffer<T>;

  using buffer_ptr = spsc_buffer_ptr<T>;

  explicit consumer_resource(buffer_ptr buf) {
    ctrl_.emplace(std::move(buf));
  }

  consumer_resource() = default;

  consumer_resource(consumer_resource&&) = default;

  consumer_resource(const consumer_resource&) = default;

  consumer_resource& operator=(consumer_resource&&) = default;

  consumer_resource& operator=(const consumer_resource&) = default;

  consumer_resource& operator=(std::nullptr_t) {
    ctrl_ = nullptr;
    return *this;
  }

  /// Tries to open the resource for reading from the buffer. The first `open`
  /// wins on concurrent access.
  /// @returns a pointer to the buffer on success, `nullptr` otherwise.
  buffer_ptr try_open() {
    if (ctrl_) {
      auto res = ctrl_->try_open();
      ctrl_ = nullptr;
      return res;
    } else {
      return nullptr;
    }
  }

  /// Convenience function for calling
  /// `ctx->make_observable().from_resource(*this)`.
  template <class Coordinator>
  auto observe_on(Coordinator* ctx) const {
    return ctx->make_observable().from_resource(*this);
  }

  /// Creates a buffer consumer for `self` and calls `on_wakeup` whenever the
  /// producer emits a wakeup signal. The actor will automatically watch the
  /// consumer, i.e., the actor will not terminate (unless forced to) until the
  /// consumer is disposed.
  /// @param self The actor that owns the consumer.
  /// @param on_wakeup A callback with signature
  ///                  `void(spsc_buffer_consumer<T>&)` that will be called
  ///                  whenever the producer emits a wakeup signal.
  /// @returns A reference-counted pointer to the consumer or `nullptr` if the
  ///          resource is not valid.
  template <class Actor, class OnWakeup>
  [[nodiscard]] spsc_buffer_consumer_ptr<T>
  consume_on(Actor* self, OnWakeup on_wakeup) {
    if (auto buf = try_open()) {
      auto res = make_counted<spsc_buffer_consumer<T>>(self->ctrl(), buf,
                                                       std::move(on_wakeup));
      buf->set_consumer(res);
      self->watch(res->as_disposable());
      return res;
    }
    return nullptr;
  }

  /// Calls `try_open` and on success immediately calls `cancel` on the buffer.
  void cancel() {
    if (auto buf = try_open())
      buf->cancel();
  }

  [[nodiscard]] bool valid() const noexcept {
    return ctrl_ != nullptr;
  }

  explicit operator bool() const noexcept {
    return valid();
  }

  bool operator!() const noexcept {
    return !valid();
  }

  friend bool operator==(const consumer_resource& lhs,
                         const consumer_resource& rhs) {
    return lhs.ctrl_ == rhs.ctrl_;
  }

  friend bool operator!=(const consumer_resource& lhs,
                         const consumer_resource& rhs) {
    return lhs.ctrl_ != rhs.ctrl_;
  }

private:
  intrusive_ptr<resource_ctrl<T, false>> ctrl_;
};

/// Produces data to an `spsc_buffer`.
template <class T>
class spsc_buffer_producer : public ref_counted,
                             public action::impl,
                             public producer {
public:
  using on_demand_signature = void(spsc_buffer_producer<T>&, size_t);

  using on_cancel_signature = void(spsc_buffer_producer<T>&);

  template <class OnDemand, class OnCancel>
  spsc_buffer_producer(caf::strong_actor_ptr owner, spsc_buffer_ptr<T> buf,
                       OnDemand on_demand, OnCancel on_cancel)
    : owner_(std::move(owner)), buf_(std::move(buf)) {
    using on_demand_impl_t = callback_impl<OnDemand, on_demand_signature>;
    on_demand_ = std::make_shared<on_demand_impl_t>(std::move(on_demand));
    using on_cancel_impl_t = callback_impl<OnCancel, on_cancel_signature>;
    on_cancel_ = std::make_shared<on_cancel_impl_t>(std::move(on_cancel));
  }

  ~spsc_buffer_producer() override {
    if (buf_) {
      buf_->close();
    }
  }

  /// Appends to the asynchronous buffer.
  /// @returns the remaining capacity after inserting the items.
  size_t push(std::span<const T> items) {
    buffer_ptr_t buf;
    {
      std::unique_lock guard{mtx_};
      buf = buf_;
    }
    if (buf) {
      return buf->push(items);
    }
    return 0;
  }

  /// Appends to the asynchronous buffer.
  /// @returns the remaining capacity after inserting the items.
  size_t push(const T& item) {
    return push(std::span{&item, 1});
  }

  bool disposed() const noexcept override {
    std::unique_lock guard{mtx_};
    return !owner_;
  }

  void dispose() override {
    buffer_ptr_t buf;
    {
      std::unique_lock guard{mtx_};
      owner_ = nullptr;
      buf.swap(buf_);
      on_demand_ = nullptr;
      on_cancel_ = nullptr;
    }
    if (buf) {
      buf->close();
    }
  }

  void abort(error reason) {
    buffer_ptr_t buf;
    {
      std::unique_lock guard{mtx_};
      if (!buf_) {
        return;
      }
      owner_ = nullptr;
      buf.swap(buf_);
      on_demand_ = nullptr;
      on_cancel_ = nullptr;
    }
    buf->abort(std::move(reason));
  }

  void on_consumer_ready() override {
    // nop
  }

  void on_consumer_cancel() override {
    std::unique_lock guard{mtx_};
    if (owner_ && pending_demand_ >= 0) {
      pending_demand_ = -1;
      owner_->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           action{this}),
                      nullptr);
    }
  }

  void on_consumer_demand(size_t demand) override {
    std::unique_lock guard{mtx_};
    if (owner_ && pending_demand_ >= 0) {
      pending_demand_ += static_cast<ptrdiff_t>(demand);
      owner_->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           action{this}),
                      nullptr);
    }
  }

  void ref_producer() const noexcept override {
    ref();
  }

  void deref_producer() const noexcept override {
    deref();
  }

  action::state current_state() const noexcept override {
    std::unique_lock guard{mtx_};
    return owner_ ? action::state::scheduled : action::state::disposed;
  }

  void resume(scheduler*, uint64_t) override {
    on_demand_t on_demand;
    size_t demand = 0;
    buffer_ptr_t buf;
    on_cancel_t on_cancel;
    {
      std::unique_lock guard{mtx_};
      if (pending_demand_ > 0) {
        on_demand = on_demand_;
        demand = pending_demand_;
        pending_demand_ = 0;
      } else if (pending_demand_ < 0) {
        owner_ = nullptr;
        buf.swap(buf_);
        on_demand_ = nullptr;
        on_cancel.swap(on_cancel_);
      }
    }
    if (on_demand) {
      (*on_demand)(*this, demand);
    } else if (on_cancel) {
      (*on_cancel)(*this);
      if (buf) {
        buf->close();
      }
    }
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  friend void intrusive_ptr_add_ref(const spsc_buffer_producer* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const spsc_buffer_producer* ptr) noexcept {
    ptr->deref();
  }

private:
  using buffer_ptr_t = spsc_buffer_ptr<T>;

  using on_demand_t = shared_callback_ptr<on_demand_signature>;

  using on_cancel_t = shared_callback_ptr<on_cancel_signature>;

  /// Guards access to the state of the consumer.
  mutable std::mutex mtx_;

  /// The actor that owns this consumer.
  caf::strong_actor_ptr owner_;

  /// The decorated buffer.
  buffer_ptr_t buf_;

  /// Callback for handling demand from the consumer.
  on_demand_t on_demand_;

  /// Callback for handling cancellation from the consumer.
  on_cancel_t on_cancel_;

  /// Stores the demand from the consumer. Set to `-1` if the consumer has
  /// canceled.
  ptrdiff_t pending_demand_ = 0;
};

/// @relates spsc_buffer_producer
template <class T>
using spsc_buffer_producer_ptr = intrusive_ptr<spsc_buffer_producer<T>>;

/// Grants access to a buffer to the first producer that calls `open`. Aborts
/// writes on the buffer if the resources gets destroyed before opening it.
/// @relates spsc_buffer
template <class T>
class producer_resource {
public:
  using value_type = T;

  using buffer_type = spsc_buffer<T>;

  using buffer_ptr = spsc_buffer_ptr<T>;

  explicit producer_resource(buffer_ptr buf) {
    ctrl_.emplace(std::move(buf));
  }

  producer_resource() = default;

  producer_resource(producer_resource&&) = default;

  producer_resource(const producer_resource&) = default;

  producer_resource& operator=(producer_resource&&) = default;

  producer_resource& operator=(const producer_resource&) = default;

  producer_resource& operator=(std::nullptr_t) {
    ctrl_ = nullptr;
    return *this;
  }

  /// Tries to open the resource for writing to the buffer. The first `open`
  /// wins on concurrent access.
  /// @returns a pointer to the buffer on success, `nullptr` otherwise.
  buffer_ptr try_open() {
    if (ctrl_) {
      auto res = ctrl_->try_open();
      ctrl_ = nullptr;
      return res;
    } else {
      return nullptr;
    }
  }

  template <class Actor, class OnDemand, class OnCancel>
  [[nodiscard]] spsc_buffer_producer_ptr<T>
  produce_on(Actor* self, OnDemand on_demand, OnCancel on_cancel) {
    if (auto buf = try_open()) {
      auto res = make_counted<spsc_buffer_producer<T>>(self->ctrl(), buf,
                                                       std::move(on_demand),
                                                       std::move(on_cancel));
      buf->set_producer(res);
      self->watch(res->as_disposable());
      return res;
    }
    return nullptr;
  }

  /// Calls `try_open` and on success immediately calls `close` on the buffer.
  void close() {
    if (auto buf = try_open())
      buf->close();
  }

  /// Calls `try_open` and on success immediately calls `abort` on the buffer.
  void abort(error reason) {
    if (auto buf = try_open())
      buf->abort(std::move(reason));
  }

  [[nodiscard]] bool valid() const noexcept {
    return ctrl_ != nullptr;
  }

  explicit operator bool() const noexcept {
    return valid();
  }

  bool operator!() const noexcept {
    return !valid();
  }

  friend bool operator==(const producer_resource& lhs,
                         const producer_resource& rhs) {
    return lhs.ctrl_ == rhs.ctrl_;
  }

  friend bool operator!=(const producer_resource& lhs,
                         const producer_resource& rhs) {
    return lhs.ctrl_ != rhs.ctrl_;
  }

private:
  intrusive_ptr<resource_ctrl<T, true>> ctrl_;
};

template <class T1, class T2 = T1>
using resource_pair = std::pair<consumer_resource<T1>, producer_resource<T2>>;

/// Creates an @ref spsc_buffer and returns two resources connected by that
/// buffer.
template <class T>
resource_pair<T>
make_spsc_buffer_resource(size_t buffer_size, size_t min_request_size) {
  using buffer_type = spsc_buffer<T>;
  auto buf = make_counted<buffer_type>(buffer_size, min_request_size);
  return {async::consumer_resource<T>{buf}, async::producer_resource<T>{buf}};
}

/// Creates an @ref spsc_buffer and returns two resources connected by that
/// buffer.
template <class T>
resource_pair<T> make_spsc_buffer_resource() {
  return make_spsc_buffer_resource<T>(defaults::flow::buffer_size,
                                      defaults::flow::min_demand);
}

} // namespace caf::async
