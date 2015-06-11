#include "caf/config.hpp"
#include "caf/detail/get_mac_addresses.hpp"

#if defined(CAF_MACOS) || defined(CAF_BSD) || defined(CAF_IOS)

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <sstream>

#include <iostream>

namespace caf {
namespace detail {

std::vector<iface_info> get_mac_addresses() {
  int mib[6];
  std::vector<iface_info> result;
  mib[0] = CTL_NET;
  mib[1] = AF_ROUTE;
  mib[2] = 0;
  mib[3] = AF_LINK;
  mib[4] = NET_RT_IFLIST;
  auto indices = if_nameindex();
  std::unique_ptr<char> buf;
  size_t buf_size = 0;
  for (auto i = indices; !(i->if_index == 0 && i->if_name == nullptr); ++i) {
    mib[5] = static_cast<int>(i->if_index);
    size_t len;
    if (sysctl(mib, 6, nullptr, &len, nullptr, 0) < 0) {
      perror("sysctl 1 error");
      exit(3);
    }
    if (buf_size < len) {
      buf.reset(new char[len]);
      buf_size = len;
    }
    if (sysctl(mib, 6, buf.get(), &len, nullptr, 0) < 0) {
      perror("sysctl 2 error");
      exit(5);
    }
    auto ifm = reinterpret_cast<if_msghdr*>(buf.get());
    auto sdl = reinterpret_cast<sockaddr_dl*>(ifm + 1);
    auto ptr = reinterpret_cast<unsigned char*>(LLADDR(sdl));
    auto uctoi = [](unsigned char c) -> unsigned {
      return static_cast<unsigned char>(c);
    };
    std::ostringstream oss;
    oss << std::hex;
    oss.fill('0');
    oss.width(2);
    oss << uctoi(*ptr++);
    for (auto j = 0; j < 5; ++j) {
      oss << ":";
      oss.width(2);
      oss << uctoi(*ptr++);
    }
    auto addr = oss.str();
    if (addr != "00:00:00:00:00:00") {
      result.push_back({i->if_name, std::move(addr)});
    }
  }
  if_freenameindex(indices);
  return result;
}

} // namespace detail
} // namespace caf

#elif defined(CAF_LINUX) || defined(CAF_ANDROID)

#include <vector>
#include <string>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <cstring>
#include <unistd.h>
#include <iostream>

namespace caf {
namespace detail {

std::vector<iface_info> get_mac_addresses() {
  // get a socket handle
  int sck = socket(AF_INET, SOCK_DGRAM, 0);
  if (sck < 0) {
    perror("socket");
    return {};
  }
  // query available interfaces
  char buf[1024] = {0};
  ifconf ifc;
  ifc.ifc_len = sizeof(buf);
  ifc.ifc_buf = buf;
  if (ioctl(sck, SIOCGIFCONF, &ifc) < 0) {
    perror("ioctl(SIOCGIFCONF)");
    return {};
  }
  std::vector<iface_info> result;
  auto ctoi = [](char c) -> unsigned {
    return static_cast<unsigned char>(c);
  };
  // iterate through interfaces
  auto ifr = ifc.ifc_req;
  auto num_ifaces = static_cast<size_t>(ifc.ifc_len) / sizeof(ifreq);
  for (size_t i = 0; i < num_ifaces; ++i) {
    auto item = &ifr[i];
    // get mac address
    if (ioctl(sck, SIOCGIFHWADDR, item) < 0) {
      perror("ioctl(SIOCGIFHWADDR)");
      return {};
    }
    std::ostringstream oss;
    oss << std::hex;
    oss.width(2);
    oss << ctoi(item->ifr_hwaddr.sa_data[0]);
    for (size_t j = 1; j < 6; ++j) {
      oss << ":";
      oss.width(2);
      oss << ctoi(item->ifr_hwaddr.sa_data[j]);
    }
    auto addr = oss.str();
    if (addr != "00:00:00:00:00:00") {
      result.push_back({item->ifr_name, std::move(addr)});
    }
  }
  return result;
}

} // namespace detail
} // namespace caf

#else

// windows

#include <ws2tcpip.h>
#include <winsock2.h>
#include <iphlpapi.h>

#include <memory>
#include <vector>
#include <string>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>

#include "caf/detail/singletons.hpp"

namespace {

constexpr size_t working_buffer_size = 15 * 1024; // 15kb by default
constexpr size_t max_iterations = 3;

struct c_free {
  template <class T>
  void operator()(T* ptr) const {
    free(ptr);
  }
};

} // namespace <anonymous>

namespace caf {
namespace detail {

std::vector<iface_info> get_mac_addresses() {
  // result vector
  std::vector<iface_info> result;
  // flags to pass to GetAdaptersAddresses
  ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
  // default to unspecified address family (both)
  ULONG family = AF_UNSPEC;
  // buffer
  std::unique_ptr<IP_ADAPTER_ADDRESSES, c_free> addresses;
  // init buf size to default, adjusted by GetAdaptersAddresses if needed
  ULONG addresses_len = working_buffer_size;
  // stores result of latest system call
  DWORD res = 0;
  // break condition
  size_t iterations = 0;
  do {
    addresses.reset((IP_ADAPTER_ADDRESSES*)malloc(addresses_len));
    if (! addresses) {
      perror("Memory allocation failed for IP_ADAPTER_ADDRESSES struct");
      exit(1);
    }
    res = GetAdaptersAddresses(family, flags, nullptr, addresses.get(),
                               &addresses_len);
  } while ((res == ERROR_BUFFER_OVERFLOW) && (++iterations < max_iterations));
  if (res == NO_ERROR) {
    // read hardware addresses from the output we've received
    for (auto addr = addresses.get(); addr != nullptr; addr = addr->Next) {
      if (addr->PhysicalAddressLength > 0) {
        std::ostringstream oss;
        oss << std::hex;
        oss.width(2);
        oss << static_cast<int>(addr->PhysicalAddress[0]);
        for (DWORD i = 1; i < addr->PhysicalAddressLength; ++i) {
          oss << ":";
          oss.width(2);
          oss << static_cast<int>(addr->PhysicalAddress[i]);
        }
        auto hw_addr = oss.str();
        if (hw_addr != "00:00:00:00:00:00") {
          result.push_back({addr->AdapterName, std::move(hw_addr)});
        }
      }
    }
  } else {
    if (res == ERROR_NO_DATA) {
      perror("No addresses were found for the requested parameters");
    } else {
      perror("Call to GetAdaptersAddresses failed with error");
    }
  }
  return result;
}

} // namespace detail
} // namespace caf

#endif
