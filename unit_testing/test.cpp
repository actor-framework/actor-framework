#include "test.hpp"

#include <cstdio>
#include <atomic>

#include "caf/all.hpp"
#include "caf/string_algorithms.hpp"

#ifdef CAF_MSVC
# include <windows.h>
#endif

using namespace std;
using namespace caf;

namespace {
atomic<size_t> s_error_count{0};
std::mutex s_stdout_mtx;
}

std::mutex& caf_stdout_mtx() {
  return s_stdout_mtx;
}

size_t caf_error_count() {
  return s_error_count;
}

void caf_inc_error_count() {
  ++s_error_count;
}

string caf_fill4(size_t value) {
  string result = to_string(value);
  while (result.size() < 4) result.insert(result.begin(), '0');
  return result;
}

const char* caf_strip_path(const char* file) {
  auto res = file;
  auto i = file;
  for (char c = *i; c != '\0'; c = *++i) {
    if (c == '/') {
      res = i + 1;
    }
  }
  return res;
}

void caf_unexpected_message(const char* file, size_t line, message t) {
  CAF_PRINTERRC(file, line, "unexpected message: " << to_string(t));
}

void caf_unexpected_timeout(const char* file, size_t line) {
  CAF_PRINTERRC(file, line, "unexpected timeout");
}

vector<string> split(const string& str, char delim, bool keep_empties) {
  using namespace std;
  vector<string> result;
  stringstream strs{str};
  string tmp;
  while (getline(strs, tmp, delim)) {
    if (!tmp.empty() || keep_empties) result.push_back(move(tmp));
  }
  return result;
}

void verbose_terminate() {
  try {
    throw;
  }
  catch (std::exception& e) {
    CAF_PRINTERR("terminate called after throwing "
            << to_verbose_string(e));
  }
  catch (...) {
    CAF_PRINTERR("terminate called after throwing an unknown exception");
  }
  abort();
}

void set_default_test_settings() {
  set_terminate(verbose_terminate);
  cout.unsetf(ios_base::unitbuf);
}

#ifdef CAF_MSVC
std::thread run_program_impl(actor rc, const char* cpath, vector<string> args) {
  string path = cpath;
  replace_all(path, "'", "\\'");
  ostringstream oss;
  oss << path;
  for (auto& arg : args) {
    oss << " " << arg;
  }
  return std::thread([](std::string cmdstr) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    CreateProcess(nullptr, // no module name
                  &cmdstr[0], // command line
                  nullptr, // process handle not inheritable
                  nullptr, // thread handle not inheritable
                  false, // no handle inheritance
                  0, // no creation flags
                  nullptr, // use parent's environment
                  nullptr, // use parent's directory
                  &si,
                  &pi);
    // be a good parent and wait for our little child
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }, oss.str());
}
#else
std::thread run_program_impl(actor rc, const char* cpath, vector<string> args) {
  string path = cpath;
  replace_all(path, "'", "\\'");
  ostringstream oss;
  oss << "'" << path << "'";
  for (auto& arg : args) {
    oss << " " << arg;
  }
  oss << " 2>&1";
  string cmdstr = oss.str();
  return std::thread([cmdstr, rc] {
    string output;
    auto fp = popen(cmdstr.c_str(), "r");
    if (!fp) {
       CAF_PRINTERR("FATAL: command line failed: " << cmdstr);
       abort();
    }
    char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
      output += buf;
    }
    pclose(fp);
    anon_send(rc, output);
  });
}
#endif
