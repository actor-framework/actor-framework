FROM ubuntu:16.04

RUN apt update -y \
    && apt upgrade -y \
    && apt install -y \
         clang-8 \
         cmake \
         git \
         libc++-8-dev \
         libc++abi-8-dev \
         libssl-dev \
         make \
    && apt autoclean

ENV CXX=/usr/bin/clang++-8
ENV CXXFLAGS=-stdlib=libc++
