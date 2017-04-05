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

#define CAF_SUITE openssl_authentication_failure
#include "caf/test/unit_test.hpp"

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
  }

  static std::string data_dir() {
     auto path = std::string(::caf::test::engine::path());
     auto found = path.find_last_of("/");
     path = path.substr(0, path.find_last_of("/"));
     path += "/../../libcaf_openssl/test";
     char rpath[PATH_MAX];
     return realpath(path.c_str(), rpath);
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

} // namespace <anonymous>

struct fixture_certs_failure {
  config server_side_config;
  actor_system server_side{server_side_config};
  config client_side_config;
  actor_system client_side{client_side_config};

  fixture_certs_failure()
	{
	server_side_config.openssl_cafile = config::data_dir() + "/ca.pem";
	server_side_config.openssl_certificate = config::data_dir() + "/cert.1.pem";
	server_side_config.openssl_key = config::data_dir() + "/key.1.enc.pem";
	server_side_config.openssl_passphrase = "12345";

	//  client_side_config.openssl_cafile = config::data_dir() + "/ca.pem";
	client_side_config.openssl_certificate = config::data_dir() + "/cert.2.pem";
	client_side_config.openssl_key = config::data_dir() + "/key.2.pem";
	}
};

CAF_TEST_FIXTURE_SCOPE(authentication_failure, fixture_certs_failure)

using openssl::remote_actor;
using openssl::publish;

CAF_TEST(authentication_failure_ping_pong) {
  // server side
  CAF_EXP_THROW(port,
                publish(server_side.spawn(make_pong_behavior), 0, local_host));
  // client side
  CAF_EXP_THROW(pong, remote_actor(client_side, local_host, port));
  client_side.spawn(make_ping_behavior, pong);
}

CAF_TEST_FIXTURE_SCOPE_END()
