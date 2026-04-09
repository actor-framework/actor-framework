# CAF: The C++ Actor Framework

CAF is an open source framework that offers a programming environment based on
the Actor Model of computation. CAF features lightweight & fast actor
implementations, data flows, HTTP and WebSocket support, pattern matching for
messages, metrics, distributed actors, and more.

## Cursor Cloud specific instructions

### Compiler

The default `c++` alternative on the VM points to clang++, which cannot link
against libstdc++ in this environment. Always configure with `CXX=g++`:

```
CXX=g++ ./configure --dev-mode
```

### Build & test

See `.cursor/rules/general.mdc` for the canonical build/test commands. Use
`--dev-mode` which enables Debug, ASAN/UBSAN, robot tests, and runtime checks.

```
cmake --build build -j $(nproc)
ctest --output-on-failure --no-tests=error --test-dir build
```

### Quality / lint checks

Before filing a pull request on GitHub, run `./scripts/local-checks.sh`. This
script runs various checks that will also run on CI.

### Robot (system) tests

Enabled by `--dev-mode`. Python dependencies are listed in `robot/dependencies.txt`. Robot tests run as part of `ctest` alongside unit tests.

### Notes

- The `./configure` script is a convenience wrapper around CMake; it clears `CMakeCache.txt` on re-runs.
- `CTEST_OUTPUT_ON_FAILURE=ON` is set in the devcontainer; pass `--output-on-failure` explicitly in Cloud.
