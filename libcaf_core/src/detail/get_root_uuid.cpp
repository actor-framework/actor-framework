// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/get_root_uuid.hpp"
#include "caf/config.hpp"

#ifndef CAF_MACOS // not needed on Mac OS X
namespace {
constexpr char uuid_format[] = "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF";
} // namespace
#endif // CAF_MACOS

#if defined(CAF_MACOS)

namespace {

inline void erase_trailing_newline(std::string& str) {
  while (!str.empty() && (*str.rbegin()) == '\n') {
    str.resize(str.size() - 1);
  }
}

constexpr const char* s_get_uuid = "/usr/sbin/diskutil info / | "
                                   "/usr/bin/awk '$0 ~ /UUID/ { print $3 }'";

} // namespace

namespace caf {
namespace detail {

std::string get_root_uuid() {
  char cbuf[100];
  // fetch hd serial
  std::string uuid;
  FILE* get_uuid_cmd = popen(s_get_uuid, "r");
  while (fgets(cbuf, 100, get_uuid_cmd) != nullptr) {
    uuid += cbuf;
  }
  pclose(get_uuid_cmd);
  erase_trailing_newline(uuid);
  return uuid;
}

} // namespace detail
} // namespace caf

#elif defined(CAF_IOS) || defined(CAF_ANDROID) || defined(CAF_NET_BSD)

// Return a randomly-generated UUID on mobile devices or NetBSD (requires root
// access to get UUID from disk).

#  include <random>

namespace caf {
namespace detail {

std::string get_root_uuid() {
  std::random_device rd;
  std::uniform_int_distribution<int> dist(0, 15);
  std::string uuid = uuid_format;
  for (auto& c : uuid) {
    if (c != '-') {
      auto n = dist(rd);
      c = static_cast<char>((n < 10) ? n + '0' : (n - 10) + 'A');
    }
  }
  return uuid;
}

} // namespace detail
} // namespace caf

#elif defined(CAF_LINUX) || defined(CAF_BSD) || defined(CAF_CYGWIN)

#  include <algorithm>
#  include <fstream>
#  include <iostream>
#  include <iterator>
#  include <sstream>
#  include <string>
#  include <vector>

#  include "caf/string_algorithms.hpp"

using std::ifstream;
using std::string;
using std::vector;

namespace caf::detail {

namespace {

struct columns_iterator {
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::vector<string>;
  using difference_type = ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;

  columns_iterator(ifstream* s = nullptr) : fs(s) {
    // nop
  }
  vector<string>& operator*() {
    return cols;
  }
  columns_iterator& operator++() {
    string line;
    if (!std::getline(*fs, line)) {
      fs = nullptr;
    } else {
      split(cols, line, is_any_of(" "), token_compress_on);
    }
    return *this;
  }
  ifstream* fs;
  vector<string> cols;
};

bool operator==(const columns_iterator& lhs, const columns_iterator& rhs) {
  return lhs.fs == rhs.fs;
}

bool operator!=(const columns_iterator& lhs, const columns_iterator& rhs) {
  return !(lhs == rhs);
}

} // namespace

std::string get_root_uuid() {
  string uuid;
  ifstream fs;
  fs.open("/etc/fstab", std::ios_base::in);
  columns_iterator end;
  auto i = std::find_if(columns_iterator{&fs}, end,
                        [](const vector<string>& cols) {
                          return cols.size() == 6 && cols[1] == "/";
                        });
  if (i != end) {
    uuid = std::move((*i)[0]);
    const char cstr[] = {"UUID="};
    auto slen = sizeof(cstr) - 1;
    if (uuid.compare(0, slen, cstr) == 0) {
      uuid.erase(0, slen);
    }
    // UUIDs are formatted as 8-4-4-4-12 hex digits groups
    auto cpy = uuid;
    std::replace_if(cpy.begin(), cpy.end(), ::isxdigit, 'F');
    // discard invalid UUID
    if (cpy != uuid_format) {
      uuid.clear();
    }
    // "\\?\Volume{5ec70abf-058c-11e1-bdda-806e6f6e6963}\"
  }
  return uuid;
}

} // namespace caf::detail

#elif defined(CAF_WINDOWS)

#  include <algorithm>
#  include <iostream>
#  include <string>

#  include <tchar.h>
#  include <windows.h>

namespace caf {
namespace detail {

namespace {
constexpr size_t max_drive_name = MAX_PATH;
}

// if TCHAR is indeed a char, we can simply move rhs
void mv(std::string& lhs, std::string&& rhs) {
  lhs = std::move(rhs);
}

// if TCHAR is defined as WCHAR, we have to do unicode conversion
void mv(std::string& lhs, const std::basic_string<WCHAR>& rhs) {
  auto size_needed = WideCharToMultiByte(CP_UTF8, 0, rhs.c_str(),
                                         static_cast<int>(rhs.size()), nullptr,
                                         0, nullptr, nullptr);
  lhs.resize(size_needed);
  WideCharToMultiByte(CP_UTF8, 0, rhs.c_str(), static_cast<int>(rhs.size()),
                      &lhs[0], size_needed, nullptr, nullptr);
}

std::string get_root_uuid() {
  using tchar_str = std::basic_string<TCHAR>;
  std::string uuid;
  TCHAR buf[max_drive_name];      // temporary buffer for volume name
  tchar_str drive = TEXT("c:\\"); // string "template" for drive specifier
  // walk through legal drive letters, skipping floppies
  for (TCHAR i = TEXT('c'); i < TEXT('z'); i++) {
    // Stamp the drive for the appropriate letter.
    drive[0] = i;
    if (GetVolumeNameForVolumeMountPoint(drive.c_str(), buf, max_drive_name)) {
      tchar_str drive_name = buf;
      auto first = drive_name.find(TEXT("Volume{"));
      if (first != std::string::npos) {
        first += 7;
        auto last = drive_name.find(TEXT("}"), first);
        if (last != std::string::npos && last > first) {
          mv(uuid, drive_name.substr(first, last - first));
          // UUIDs are formatted as 8-4-4-4-12 hex digits groups
          auto cpy = uuid;
          std::replace_if(cpy.begin(), cpy.end(), ::isxdigit, 'F');
          // discard invalid UUID
          if (cpy != uuid_format) {
            uuid.clear();
          } else {
            return uuid; // return first valid UUID we get
          }
        }
      }
    }
  }
  return uuid;
}

} // namespace detail
} // namespace caf

#endif // CAF_WINDOWS
