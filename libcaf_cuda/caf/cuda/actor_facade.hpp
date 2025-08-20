#pragma once

#include <stdexcept>
#include <functional>
#include <tuple>
#include <queue>
#include <utility>
#include <type_traits>

#include <caf/local_actor.hpp>
#include <caf/actor.hpp>
#include <caf/response_promise.hpp>
#include <caf/scheduler.hpp>
#include <caf/resumable.hpp>

#include "caf/cuda/nd_range.hpp"
#include "caf/cuda/global.hpp"
#include "caf/cuda/program.hpp"
#include "caf/cuda/command.hpp"
#include "caf/cuda/platform.hpp"
#include <random>
#include <climits>
#include <thread>

namespace caf::cuda {

//An actor that acts as a gateway to the gpu 
//you can send it messages that is of the parameters of the kernel
//you wish to launch
//and it will reply with an output_buffer
template <bool PassConfig, class... Ts>
class actor_facade : public caf::local_actor, public caf::resumable {
public:

  //Factory methods to create the actor
  static caf::actor create(
    caf::actor_system& sys,
    caf::actor_config&& actor_conf,
    program_ptr program,
    nd_range dims,
    Ts&&... xs
  ) {
    return caf::make_actor<actor_facade<PassConfig, std::decay_t<Ts>...>, caf::actor>(
      sys.next_actor_id(),
      sys.node(),
      &sys,
      std::move(actor_conf),
      std::move(program),
      std::move(dims),
      std::forward<Ts>(xs)...);
  }

  static caf::actor create(
    caf::actor_system* sys,
    caf::actor_config&& actor_conf,
    program_ptr program,
    nd_range dims,
    Ts&&... xs
  ) {
    return caf::make_actor<actor_facade<PassConfig, std::decay_t<Ts>...>, caf::actor>(
      sys->next_actor_id(),
      sys->node(),
      sys,
      std::move(actor_conf),
      std::move(program),
      std::move(dims),
      std::forward<Ts>(xs)...);
  }

  //constructor
  actor_facade(caf::actor_config&& cfg, program_ptr prog, nd_range nd, Ts&&... xs)
    : local_actor(cfg),
      config_(std::move(cfg)),
      program_(std::move(prog)),
      dims_(nd) {
  }

  //deconstructor
  ~actor_facade() {
    auto plat = platform::create();
    plat->release_streams_for_actor(actor_id);
  }

  //creates a command and enqueues the kernel to be launched
  void create_command(program_ptr program, Ts&&... xs) {
    using command_t = command<caf::actor, Ts...>;
    auto rp = make_response_promise();
    auto cmd = make_counted<command_t>(
      program,
      dims_,
      actor_id,
      std::forward<Ts>(xs)...);
    rp.deliver(cmd->enqueue());
    //anon_mail(kernel_done_atom_v).send(caf::actor_cast<caf::actor>(this));
  }

  //does the same thing as create_command
  void run_kernel(Ts&... xs) {
    create_command(program_, std::forward<Ts>(xs)...);
  }

private:
  caf::actor_config config_;
  program_ptr program_;
  nd_range dims_;
  std::queue<mailbox_element_ptr> mailbox_;
  std::atomic<int> pending_promises_ = 0;
  std::atomic<bool> shutdown_requested_ = false;
  int actor_id = generate_id();
  std::atomic_flag resuming_flag_ = ATOMIC_FLAG_INIT;
  caf::actor self_ =   caf::actor_cast<caf::actor>(this);

  //creates an id for the actor facade, used for stream allocation and 
  //deallocation
  int generate_id() {
      return random_number();	  
  }

  //helper method that is used to handle an incoming message
  bool handle_message(const message& msg) {
    if (!msg.types().empty() && msg.types()[0] == caf::type_id_v<caf::actor>) {
      auto sender = msg.get_as<caf::actor>(0);
      if (msg.match_elements<caf::actor, Ts...>()) {
        return unpack_and_run_wrapped(sender, msg, std::index_sequence_for<Ts...>{});
      }
      if (msg.match_elements<caf::actor, raw_t<Ts>...>()) {
        return unpack_and_run(sender, msg, std::index_sequence_for<Ts...>{});
      }
    }

    if (!msg.types().empty()) { 
	    return unpack_and_run_wrapped_async(msg, std::index_sequence_for<Ts...>{});
    }
    std::cout << "[WARNING], message format not recognized by actor facade, dropping message\n";
    
     return false;
  }

  //unpacks a message and launches the kernel
  template <std::size_t... Is>
  bool unpack_and_run_wrapped(caf::actor sender, const message& msg, std::index_sequence<Is...>) {
    auto wrapped = std::make_tuple(msg.get_as<Ts>(Is + 1)...);
    run_kernel(std::get<Is>(wrapped)...);
    return true;
  }

  //unpacks a message and launches a kernel
  template <std::size_t... Is>
  bool unpack_and_run(caf::actor sender, const message& msg, std::index_sequence<Is...>) {
    auto unpacked = std::make_tuple(msg.get_as<raw_t<Ts>>(Is + 1)...);
    auto wrapped = std::make_tuple(Ts(std::get<Is>(unpacked))...);
    run_kernel(std::get<Is>(wrapped)...);
    return true;
  }


  //unpacks a message and launches a kernel
  template <std::size_t... Is>
  bool unpack_and_run_wrapped_async(const message& msg, std::index_sequence<Is...>) {
    auto wrapped = std::make_tuple(msg.get_as<Ts>(Is)...);
    run_kernel(std::get<Is>(wrapped)...);
    return true;
  }




  subtype_t subtype() const noexcept override {
    return subtype_t(0);
  }

  //handles scheduling for caf, will return based on what work needs to be done
 resumable::resume_result resume(::caf::scheduler* sched, size_t max_throughput) override {
  if (resuming_flag_.test_and_set(std::memory_order_acquire)) {
    return resumable::resume_later;
  }

  //ensure the lock is released on exit of this method 
  auto clear_flag = caf::detail::scope_guard([this] noexcept {
    resuming_flag_.clear(std::memory_order_release);
  });

  size_t processed = 0;

  while (!mailbox_.empty() && processed < max_throughput) {
    auto msg = std::move(mailbox_.front());
    mailbox_.pop();

    if (!msg || !msg->content().ptr()) {
      std::cout << "[Thread " << std::this_thread::get_id()
                << "] Dropping message with no content\n";
      continue;
    }

    pending_promises_++;
    current_mailbox_element(msg.get());

    if (msg->content().match_elements<kernel_done_atom>()) {
      if (--pending_promises_ == 0 && shutdown_requested_) {
        quit(exit_reason::user_shutdown);
        return resumable::done;
      }
      current_mailbox_element(nullptr);
      ++processed;
      continue;
    }

    //check for exit message if yes begin the shutdown process
    if (msg->content().match_elements<exit_msg>()) {
      auto exit = msg->content().get_as<exit_msg>(0);
      shutdown_requested_ = true;
      if (--pending_promises_ == 0) {
        quit(static_cast<exit_reason>(exit.reason.code()));
        return resumable::done;
      } else {
        current_mailbox_element(nullptr);
        return resumable::resume_later;
      }
    }

    //process the message and begin launching the kernel
    handle_message(msg->content());
    //pending_promises_--;
    current_mailbox_element(nullptr);
    ++processed;
  }

  // If there's still more work, return resume_later
  if (!mailbox_.empty())
    return resumable::resume_later;

  return shutdown_requested_ ? resumable::resume_later : resumable::done;
}

  void ref_resumable() const noexcept override  {}

  void deref_resumable() const noexcept override  {}

  //add a message to its mailbox and enqueue the actor on the scheduler
  bool enqueue(mailbox_element_ptr what, ::caf::scheduler* sched) override {
    if (!what || shutdown_requested_)
      return false;

    bool was_empty = mailbox_.empty();
    mailbox_.push(std::move(what));
    if (was_empty && sched) {
      sched->schedule(this);
      return true;
    }

    return false;
  }

  //schedule the actor on startup
  void launch(::caf::scheduler* sched, bool lazy, [[maybe_unused]] bool interruptible) override {
    if (!lazy && sched) {
      sched->schedule(this);
    }
  }

  void do_unstash(mailbox_element_ptr what) override {
    if (what) {
      mailbox_.push(std::move(what));
    }
  }

  //close the mailbox when done
  void force_close_mailbox() override  {
    while (!mailbox_.empty()) {
      mailbox_.pop();
    }
  }


  //helper method to be executed when an exit message is received 
  void quit(exit_reason reason) {
   self_ = nullptr; 
   force_close_mailbox();
    current_mailbox_element(nullptr);
  }
};

} // namespace caf::cuda
