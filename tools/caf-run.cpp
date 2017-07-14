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

#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

static constexpr const char base64_tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "0123456789+/";

std::string encode_base64(const string& str) {
  std::string result;
  // consumes three characters from input
  auto consume = [&](const char* i) {
    int buf[] {
      (i[0] & 0xfc) >> 2,
      ((i[0] & 0x03) << 4) + ((i[1] & 0xf0) >> 4),
      ((i[1] & 0x0f) << 2) + ((i[2] & 0xc0) >> 6),
      i[2] & 0x3f
    };
    for (auto x : buf)
      result += base64_tbl[x];
  };
  // iterate string in chunks of three characters
  auto i = str.begin();
  for ( ; std::distance(i, str.end()) >= 3; i += 3)
    consume(&(*i));
  if (i != str.end()) {
    // "fill" string with 0s
    char cbuf[] = {0, 0, 0};
    std::copy(i, str.end(), cbuf);
    consume(cbuf);
    // override filled characters (garbage) with '='
    for (auto j = result.end() - (3 - (str.size() % 3)); j != result.end(); ++j)
      *j = '=';
  }
  return result;
}

class host_desc {
public:
  std::string host;
  int cpu_slots;
  string opencl_device_ids;

  host_desc(std::string host_addr, int slots, string cldevices)
      : host(std::move(host_addr)),
        cpu_slots(slots),
        opencl_device_ids(std::move(cldevices)) {
    // nop
  }

  host_desc(host_desc&&) = default;
  host_desc(const host_desc&) = default;
  host_desc& operator=(host_desc&&) = default;
  host_desc& operator=(const host_desc&) = default;

  static void append(vector<host_desc>& xs, const string& line, size_t num) {
    vector<string> fields;
    split(fields, line, is_any_of(" "), token_compress_on);
    if (fields.empty())
      return;
    host_desc hd;
    hd.host = std::move(fields.front());
    hd.cpu_slots = 0;
    hd.opencl_device_ids = "";
    for (auto i = fields.begin() + 1; i != fields.end(); ++i) {
      if (starts_with(*i, "device_ids=")) {
        hd.opencl_device_ids.assign(std::find(i->begin(), i->end(), '=') + 1,
                                    i->end());
      } else if (sscanf(i->c_str(), "slots=%d", &hd.cpu_slots) != 0) {
        cerr << "invalid option at line " << num << ": " << *i << endl;
      }
    }
    xs.emplace_back(std::move(hd));
  }

private:
  host_desc() = default;
};

std::vector<host_desc> read_hostfile(const string& fname) {
  std::vector<host_desc> result;
  std::ifstream in{fname};
  std::string line;
  size_t line_num = 0;
  while (std::getline(in, line))
    host_desc::append(result, line, ++line_num);
  return result;
}

bool run_ssh(actor_system& system, const string& wdir,
             const string& cmd, const string& host) {
  std::cout << "runssh, wdir: " << wdir << " cmd: " << cmd
            << " host: " << host << std::endl;
  // pack command before sending it to avoid any issue with shell escaping
  string full_cmd = "cd ";
  full_cmd += wdir;
  full_cmd += '\n';
  full_cmd += cmd;
  auto packed = encode_base64(full_cmd);
  std::ostringstream oss;
  oss << "ssh -Y -o ServerAliveInterval=60 " << host
      << R"( "echo )" << packed << R"( | base64 --decode | /bin/sh")";
  //return system(oss.str().c_str());
  string line;
  std::cout << "popen: " << oss.str() << std::endl;
  auto fp = popen(oss.str().c_str(), "r");
  if (fp == nullptr)
    return false;
  char buf[512];
  auto eob = buf + sizeof(buf); // end-of-buf
  auto pred = [](char c) { return c == 0 || c == '\n'; };
  scoped_actor self{system};
  while (fgets(buf, sizeof(buf), fp) != nullptr) {
    auto i = buf;
    auto e = std::find_if(i, eob, pred);
    line.insert(line.end(), i, e);
    while (e != eob && *e != 0) {
      aout(self) << line << std::endl;
      line.clear();
      i = e + 1;
      e = std::find_if(i, eob, pred);
      line.insert(line.end(), i, e);
    }
  }
  pclose(fp);
  std::cout << "host down: " << host << std::endl;
  if (!line.empty())
    aout(self) << line << std::endl;
  return true;
}

void bootstrap(actor_system& system,
               const string& wdir,
               const host_desc& master,
               vector<host_desc> slaves,
               const string& cmd,
               vector<string> args) {
  using io::network::interfaces;
  if (!args.empty())
    args.erase(args.begin());
  scoped_actor self{system};
  // open a random port and generate a list of all
  // possible addresses slaves can use to connect to us
  auto port_res = system.middleman().publish(self, 0);
  if (!port_res) {
    cerr << "fatal: unable to publish actor: "
         << system.render(port_res.error()) << endl;
    return;
  }
  auto port = *port_res;
  // run a slave process at master host if user defined slots > 1 for it
  if (master.cpu_slots > 1)
    slaves.emplace_back(master.host, master.cpu_slots - 1,
                        master.opencl_device_ids);
  for (auto& slave : slaves) {
    using namespace caf::io::network;
    // build SSH command and pack it to avoid any issue with shell escaping
    std::thread{[=, &system](actor bootstrapper) {
      std::ostringstream oss;
      oss << cmd;
      if (slave.cpu_slots > 0)
        oss << " --caf#scheduler.max-threads=" << slave.cpu_slots;
      if (!slave.opencl_device_ids.empty())
        oss << " --caf#opencl-devices=" << slave.opencl_device_ids;
      oss << " --caf#slave-mode"
          << " --caf#slave-name=" << slave.host
          << " --caf#bootstrap-node=";
      bool is_first = true;
      interfaces::traverse({protocol::ipv4, protocol::ipv6},
                           [&](const char*, protocol, bool lo, const char* x) {
        if (lo)
          return;
        if (!is_first)
          oss << ",";
        else
          is_first = false;
        oss << x << "/" << port;
      });
      for (auto& arg : args)
        oss << " " << arg;
      if (!run_ssh(system, wdir, oss.str(), slave.host))
        anon_send(bootstrapper, slave.host);
    }, actor{self}}.detach();
  }
  std::string slaveslist;
  for (size_t i = 0; i < slaves.size(); ++i) {
    self->receive(
      [&](const string& host, uint16_t slave_port) {
        if (!slaveslist.empty())
          slaveslist += ',';
        slaveslist += host;
        slaveslist += '/';
        slaveslist += std::to_string(slave_port);
      },
      [](const string& node) {
        cerr << "unable to launch process via SSH at node " << node << endl;
      }
    );
  }
  // run (and wait for) master
  std::ostringstream oss;
  oss << cmd << " --caf#slave-nodes=" << slaveslist << " " << join(args, " ");
  run_ssh(system, wdir, oss.str(), master.host);
}

#define RETURN_WITH_ERROR(output)                                              \
  do {                                                                         \
    ::std::cerr << output << ::std::endl;                                      \
    return 1;                                                                  \
  } while (true)

int main(int argc, char** argv) {
  actor_system_config cfg;
  cfg.parse(argc, argv);
  if (cfg.cli_helptext_printed)
    return 0;
  if (cfg.slave_mode)
    RETURN_WITH_ERROR("cannot use slave mode in caf-run tool");
  string hostfile;
  std::unique_ptr<char, void (*)(void*)> pwd{getcwd(nullptr, 0), ::free};
  string wdir;
  auto res = cfg.args_remainder.extract_opts({
    {"hostfile", "path to the hostfile", hostfile},
    {"wdir", wdir}
  });
  if (hostfile.empty())
    RETURN_WITH_ERROR("no hostfile specified or hostfile is empty");
  auto& remainder = res.remainder;
  if (remainder.empty())
    RETURN_WITH_ERROR("empty command line");
  auto cmd = std::move(remainder.get_mutable_as<std::string>(0));
  vector<string> xs;
  remainder.drop(1).extract([&](string& x) { xs.emplace_back(std::move(x)); });
  auto hosts = read_hostfile(hostfile);
  if (hosts.empty())
    RETURN_WITH_ERROR("no valid entry in hostfile");
  actor_system system{cfg};
  auto master = hosts.front();
  hosts.erase(hosts.begin());
  bootstrap(system, (wdir.empty()) ? pwd.get() : wdir.c_str(), master,
            std::move(hosts), cmd, xs);
}
