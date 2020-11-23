FROM ubuntu:16.04

RUN \
  apt update -y \
  && apt upgrade -y \
  && apt install -y \
       clang-8 \
       libc++-8-dev \
       libc++abi-8-dev \
       cmake \
       git \
       libssl-dev \
       make \
  && apt autoclean

ENV CXX=/usr/bin/clang++-8
ENV CXXFLAGS=-stdlib=libc++
