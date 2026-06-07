FROM debian:12

# Base setup
RUN apt-get update -y \
    && apt-get install -y --no-install-recommends \
      ca-certificates \
      clangd \
      cmake \
      cppcheck \
      curl \
      g++ \
      git \
      jq \
      libssl-dev \
      make \
      ninja-build \
      python3 \
      python3-pip \
      vim

# Install Robot dependencies
COPY robot/dependencies.txt /tmp/robot-dependencies.txt
RUN python3 -m pip install -r /tmp/robot-dependencies.txt --break-system-packages

# Install clang-format 22 (same version as used in CI).
RUN python3 -m pip install --break-system-package clang-format==22.1.5

# Have CMake use Ninja as the default generator
ENV CMAKE_GENERATOR=Ninja

# Set a sane default for CTest to show output on failure
ENV CTEST_OUTPUT_ON_FAILURE=ON
