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

#include "caf/io/network/test_multiplexer.hpp"

namespace caf {
namespace io {
namespace network {

test_multiplexer::~test_multiplexer() {
  // nop
}

connection_handle test_multiplexer::new_tcp_scribe(const std::string& host,
                                                   uint16_t port) {
  connection_handle result;
  auto i = scribes_.find(std::make_pair(host, port));
  if (i != scribes_.end()) {
    result = i->second;
    scribes_.erase(i);
  }
  return result;
}

void test_multiplexer::assign_tcp_scribe(abstract_broker* ptr,
                                         connection_handle hdl) {
  class impl : public abstract_broker::scribe {
  public:
    impl(abstract_broker* self, connection_handle ch, test_multiplexer* mpx)
        : abstract_broker::scribe(self, ch),
          mpx_(mpx) {
      // nop
    }

    void configure_read(receive_policy::config config) {
      mpx_->read_config(hdl()) = config;
    }

    abstract_broker::buffer_type& wr_buf() {
      return mpx_->output_buffer(hdl());
    }

    abstract_broker::buffer_type& rd_buf() {
      return mpx_->input_buffer(hdl());
    }

    void stop_reading() {
      mpx_->stopped_reading(hdl()) = true;
      disconnect(false);
    }

    void flush() {
      // nop
    }

  private:
    test_multiplexer* mpx_;
  };
  auto sptr = make_counted<impl>(ptr, hdl, this);
  impl_ptr(hdl) = sptr;
  ptr->add_scribe(sptr);
}

connection_handle test_multiplexer::add_tcp_scribe(abstract_broker*,
                                                   native_socket) {
  std::cerr << "test_multiplexer::add_tcp_scribe called with native socket"
            << std::endl;
  abort();
}

connection_handle test_multiplexer::add_tcp_scribe(abstract_broker* ptr,
                                                   const std::string& host,
                                                   uint16_t port) {
  auto hdl = new_tcp_scribe(host, port);
  if (hdl != invalid_connection_handle)
    assign_tcp_scribe(ptr, hdl);
  return hdl;
}

std::pair<accept_handle, uint16_t>
test_multiplexer::new_tcp_doorman(uint16_t port, const char*, bool) {
  accept_handle result;
  auto i = doormen_.find(port);
  if (i != doormen_.end()) {
    result = i->second;
    doormen_.erase(i);
  }
  return {result, port};
}

void test_multiplexer::assign_tcp_doorman(abstract_broker* ptr,
                                          accept_handle hdl) {
  class impl : public abstract_broker::doorman {
  public:
    impl(abstract_broker* self, accept_handle ah, test_multiplexer* mpx)
        : doorman(self, ah, mpx->port(ah)),
          mpx_(mpx) {
      // nop
    }

    void new_connection() {
      auto& mm = mpx_->pending_connects();
      auto i = mm.find(hdl());
      if (i != mm.end()) {
        accept_msg().handle = i->second;
        parent()->invoke_message(invalid_actor_addr, invalid_message_id,
                                 accept_msg_);
        mm.erase(i);
      }
    }

    void stop_reading() {
      mpx_->stopped_reading(hdl()) = true;
      disconnect(false);
    }

    void launch() {
      // nop
    }

  private:
    test_multiplexer* mpx_;
  };
  auto dptr = make_counted<impl>(ptr, hdl, this);
  impl_ptr(hdl) = dptr;
  ptr->add_doorman(dptr);
}

accept_handle test_multiplexer::add_tcp_doorman(abstract_broker*,
                                                native_socket) {
  std::cerr << "test_multiplexer::add_tcp_doorman called with native socket"
            << std::endl;
  abort();
}

std::pair<accept_handle, uint16_t>
test_multiplexer::add_tcp_doorman(abstract_broker* ptr, uint16_t prt,
                                  const char* in, bool reuse_addr) {
  auto result = new_tcp_doorman(prt, in, reuse_addr);
  if (result.first != invalid_accept_handle) {
    port(result.first) = prt;
    assign_tcp_doorman(ptr, result.first);
  }
  return result;
}

auto test_multiplexer::make_supervisor() -> supervisor_ptr {
  // not needed
  return nullptr;
}

void test_multiplexer::run() {
  // nop
}

void test_multiplexer::provide_scribe(std::string host, uint16_t port,
                                      connection_handle hdl) {
  scribes_.emplace(std::make_pair(std::move(host), port), hdl);
}

void test_multiplexer::provide_acceptor(uint16_t port, accept_handle hdl) {
  doormen_.emplace(port, hdl);
  doorman_data_[hdl].port = port;
}

/// The external input buffer should be filled by
/// the test program.
test_multiplexer::buffer_type&
test_multiplexer::virtual_network_buffer(connection_handle hdl) {
  return scribe_data_[hdl].xbuf;
}

test_multiplexer::buffer_type&
test_multiplexer::output_buffer(connection_handle hdl) {
  return scribe_data_[hdl].wr_buf;
}

test_multiplexer::buffer_type&
test_multiplexer::input_buffer(connection_handle hdl) {
  return scribe_data_[hdl].rd_buf;
}

receive_policy::config& test_multiplexer::read_config(connection_handle hdl) {
  return scribe_data_[hdl].recv_conf;
}

bool& test_multiplexer::stopped_reading(connection_handle hdl) {
  return scribe_data_[hdl].stopped_reading;
}

abstract_broker::scribe_ptr& test_multiplexer::impl_ptr(connection_handle hdl) {
  return scribe_data_[hdl].ptr;
}

uint16_t& test_multiplexer::port(accept_handle hdl) {
  return doorman_data_[hdl].port;
}

bool& test_multiplexer::stopped_reading(accept_handle hdl) {
  return doorman_data_[hdl].stopped_reading;
}

abstract_broker::doorman_ptr& test_multiplexer::impl_ptr(accept_handle hdl) {
  return doorman_data_[hdl].ptr;
}

void test_multiplexer::add_pending_connect(accept_handle src,
                                           connection_handle hdl) {
  pending_connects_.emplace(src, hdl);
}

test_multiplexer::pending_connects_map& test_multiplexer::pending_connects() {
  return pending_connects_;
}

test_multiplexer::pending_scribes_map& test_multiplexer::pending_scribes() {
  return scribes_;
}


void test_multiplexer::accept_connection(accept_handle hdl) {
  auto& dd = doorman_data_[hdl];
  if (dd.ptr == nullptr)
    throw std::logic_error("accept_connection: this doorman was not "
                           "assigned to a broker yet");
  dd.ptr->new_connection();
}

void test_multiplexer::read_data(connection_handle hdl) {
  scribe_data& sd = scribe_data_[hdl];
  while (sd.xbuf.size() >= sd.recv_conf.second) {
    sd.rd_buf.clear();
    auto first = sd.xbuf.begin();
    auto last = first + static_cast<ptrdiff_t>(sd.recv_conf.second);
    sd.rd_buf.insert(sd.rd_buf.end(), first, last);
    sd.xbuf.erase(first, last);
    sd.ptr->consume(sd.rd_buf.data(), sd.rd_buf.size());
  }
}

void test_multiplexer::virtual_send(connection_handle hdl,
                                    const buffer_type& buf) {
  auto& vb = virtual_network_buffer(hdl);
  vb.insert(vb.end(), buf.begin(), buf.end());
  read_data(hdl);
}

void test_multiplexer::exec_runnable() {
  runnable_ptr ptr;
  { // critical section
    guard_type guard{mx_};
    while (runnables_.empty())
      cv_.wait(guard);
    runnables_.front().swap(ptr);
    runnables_.pop_front();
  }
  ptr->run();
}

bool test_multiplexer::try_exec_runnable() {
  runnable_ptr ptr;
  { // critical section
    guard_type guard{mx_};
    if (runnables_.empty())
      return false;
    runnables_.front().swap(ptr);
    runnables_.pop_front();
  }
  ptr->run();
  return true;
}

void test_multiplexer::dispatch_runnable(runnable_ptr ptr) {
  std::list<runnable_ptr> tmp;
  tmp.emplace_back(std::move(ptr));
  guard_type guard{mx_};
  runnables_.splice(runnables_.end(), std::move(tmp));
  cv_.notify_all();
}

} // namespace network
} // namespace io
} // namespace caf
