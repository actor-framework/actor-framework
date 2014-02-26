#include "cppa/config.hpp"
#include "cppa/util/get_mac_addresses.hpp"

#ifdef CPPA_MACOS

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

namespace cppa { namespace util {

std::vector<std::string> get_mac_addresses() {
    int mib[6];

    std::vector<std::string> result;

    mib[0] = CTL_NET;
    mib[1] = AF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_LINK;
    mib[4] = NET_RT_IFLIST;

    auto indices = if_nameindex();

    std::unique_ptr<char> buf;
    size_t buf_size = 0;

    for (auto i = indices; !(i->if_index == 0 && i->if_name == nullptr); ++i) {
        mib[5] = i->if_index;

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

        auto ctoi = [](char c) -> unsigned {
            return static_cast<unsigned char>(c);
        };

        std::ostringstream oss;
        oss << std::hex;
        oss.fill('0');
        oss.width(2);
        oss << ctoi(*ptr++);
        for (auto j = 0; j < 5; ++j) {
            oss << ":";
            oss.width(2);
            oss << ctoi(*ptr++);
        }
        auto addr = oss.str();
        if (addr != "00:00:00:00:00:00") result.push_back(std::move(addr));
    }
    if_freenameindex(indices);
    return result;
}

} } // namespace cppa::util

#elif defined(CPPA_LINUX)


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

using namespace std;

namespace cppa { namespace util {

std::vector<std::string> get_mac_addresses() {
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

    vector<string> hw_addresses;
    auto ctoi = [](char c) -> unsigned {
        return static_cast<unsigned char>(c);
    };
    // iterate through interfaces
    auto ifr = ifc.ifc_req;
    auto num_ifaces = ifc.ifc_len / sizeof(struct ifreq);
    for (size_t i = 0; i < num_ifaces; ++i) {
        auto item = &ifr[i];
        // get mac address
        if (ioctl(sck, SIOCGIFHWADDR, item) < 0) {
            perror("ioctl(SIOCGIFHWADDR)");
            return {};
        }
        std::ostringstream oss;
        oss << hex;
        oss.width(2);
        oss << ctoi(item->ifr_hwaddr.sa_data[0]);
        for (size_t i = 1; i < 6; ++i) {
            oss << ":";
            oss.width(2);
            oss << ctoi(item->ifr_hwaddr.sa_data[i]);
        }
        auto addr = oss.str();
        if (addr != "00:00:00:00:00:00") {
            hw_addresses.push_back(std::move(addr));
        }
    }
    return hw_addresses;
}

} } // namespace cppa::util

#else

// windows

#include <winsock2.h>
#include <vector>
#include <string>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <stdio.h>

#include <cstring>
//#include <unistd.h>
#include <iostream>
#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
// Link with Iphlpapi.lib
// #pragma comment(lib, "IPHLPAPI.lib")    -liphlpapi

#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

/* Note: could also use malloc() and free() */


using namespace std;

namespace cppa { namespace util {

std::vector<std::string> get_mac_addresses() {

   /* Declare and initialize variables */

    DWORD dwRetVal = 0;

    unsigned int i = 0;

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

    // default to unspecified address family (both)
    ULONG family = AF_UNSPEC;


    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    ULONG outBufLen = 0;
    ULONG Iterations = 0;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;

    vector<string> hw_addresses;

// Allocate a 15 KB buffer to start with.
    outBufLen = WORKING_BUFFER_SIZE;

    do {

        pAddresses = (IP_ADAPTER_ADDRESSES *) MALLOC(outBufLen);
        if (pAddresses == NULL) {
            perror("Memory allocation failed for IP_ADAPTER_ADDRESSES struct");
            exit(1);
        }

        dwRetVal =
            GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            FREE(pAddresses);
            pAddresses = NULL;
        } else {
            break;
        }

        Iterations++;

    } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));


    if (dwRetVal == NO_ERROR) {
        // If successful, output some information from the data we received
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            if (pCurrAddresses->PhysicalAddressLength != 0) {
                std::ostringstream oss;
                oss << hex;
                for (i = 0; i < (unsigned int) pCurrAddresses->PhysicalAddressLength; i++) {
                    if (i == (pCurrAddresses->PhysicalAddressLength - 1)) {
                        oss.width(2);
                        oss << ((int) pCurrAddresses->PhysicalAddress[i]);
                    } else {
                        oss.width(2);
                        oss << ((int) pCurrAddresses->PhysicalAddress[i]) << ":";
                       
                    }

                }
                auto addr = oss.str();
                if (addr != "00:00:00:00:00:00") {
                    hw_addresses.push_back(std::move(addr));
                }

            }

            pCurrAddresses = pCurrAddresses->Next;
        }
    } else {

        if (dwRetVal == ERROR_NO_DATA) {
            perror("No addresses were found for the requested parameters");
        } else {
            perror("Call to GetAdaptersAddresses failed with error");
        }
    }

    if (pAddresses) {
        FREE(pAddresses);
    }

    return hw_addresses;
}

} } // namespace cppa::util


#endif
