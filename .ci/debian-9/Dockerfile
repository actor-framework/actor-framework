FROM debian:9

RUN apt update -y && \
  apt upgrade -y

RUN apt install -y \
  clang-7 \
  libc++-7-dev \
  libc++abi-7-dev \
  cmake \
  git \
  libssl-dev \
  make \
 && apt autoclean

ENV CXX=/usr/bin/clang++-7
ENV CXXFLAGS=-stdlib=libc++
