/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/config.hpp"

#define CAF_SUITE openssl_authentication
#include "caf/test/unit_test.hpp"

#include <unistd.h>

#include <vector>
#include <sstream>
#include <utility>
#include <algorithm>

#include <limits.h>
#include <stdlib.h>

#include "caf/all.hpp"
#include "caf/io/all.hpp"
#include "caf/openssl/all.hpp"

using namespace caf;

namespace {

constexpr char local_host[] = "127.0.0.1";

class config : public actor_system_config {
public:
  config() {
    load<io::middleman>();
    load<openssl::manager>();
    add_message_type<std::vector<int>>("std::vector<int>");
    actor_system_config::parse(test::engine::argc(),
                               test::engine::argv());
    scheduler_max_threads = 1;
  }

  static std::string data_dir() {
     std::string path{::caf::test::engine::path()};
     path = path.substr(0, path.find_last_of("/"));
     // TODO: https://github.com/actor-framework/actor-framework/issues/555
     path += "/../../libcaf_openssl/test";
     char rpath[PATH_MAX];
     auto rp = realpath(path.c_str(), rpath);
     std::string result;
     if (rp)
       result = rp;
     return result;
  }
};

behavior make_pong_behavior() {
  return {
    [](int val) -> int {
      CAF_MESSAGE("pong with " << ++val);
      return val;
    }
  };
}

behavior make_ping_behavior(event_based_actor* self, const actor& pong) {
  CAF_MESSAGE("ping with " << 0);
  self->send(pong, 0);
  return {
    [=](int val) -> int {
      if (val == 3) {
        CAF_MESSAGE("ping with exit");
        self->send_exit(self->current_sender(),
                        exit_reason::user_shutdown);
        CAF_MESSAGE("ping quits");
        self->quit();
      }
      CAF_MESSAGE("ping with " << val);
      return val;
    }
  };
}

struct fixture {
  config server_side_config;
  config client_side_config;
  bool initialized;
  union { actor_system server_side; };
  union { actor_system client_side; };

  fixture() : initialized(false) {
    // nop
  }

  ~fixture() {
    if (initialized) {
      server_side.~actor_system();
      client_side.~actor_system();
    }
  }

  bool init(bool skip_client_side_ca) {
    auto cd = config::data_dir();
    cd += '/';
    server_side_config.openssl_passphrase = "12345";
    // check whether all files exist before setting config parameters
    std::string dummy;
    std::pair<const char*, std::string*> cfg[] {
      {"ca.pem", &server_side_config.openssl_cafile},
      {"cert.1.pem", &server_side_config.openssl_certificate},
      {"key.1.enc.pem", &server_side_config.openssl_key},
      {"ca.pem", skip_client_side_ca ? &dummy
                                     : &client_side_config.openssl_cafile},
      {"cert.2.pem", &client_side_config.openssl_certificate},
      {"key.2.pem", &client_side_config.openssl_key}
    };
    // return if any file is unreadable or non-existend
    for (auto& x : cfg) {
      auto path = cd + x.first;
      if (access(path.c_str(), F_OK) == -1) {
        CAF_MESSAGE("pem files missing, skip test");
        return false;
      }
      *x.second = std::move(path);
    }
    new (&server_side) actor_system(server_side_config);
    new (&client_side) actor_system(client_side_config);
    initialized = true;
    return true;
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(authentication, fixture)

using openssl::remote_actor;
using openssl::publish;

CAF_TEST(authentication_succes) {
  if (!init(false))
    return;
  // server side
  CAF_EXP_THROW(port,
                publish(server_side.spawn(make_pong_behavior), 0, local_host));
  // client side
  CAF_EXP_THROW(pong, remote_actor(client_side, local_host, port));
  client_side.spawn(make_ping_behavior, pong);
}

CAF_TEST(authentication_failure) {
  if (!init(true))
    return;
  // server side
  auto pong = server_side.spawn(make_pong_behavior);
  CAF_EXP_THROW(port, publish(pong, 0, local_host));
  // client side
  auto remote_pong = remote_actor(client_side, local_host, port);
  CAF_REQUIRE(!remote_pong);
  anon_send_exit(pong, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
