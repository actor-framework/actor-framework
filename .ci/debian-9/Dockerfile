FROM debian:9

RUN apt update -y \
    && apt upgrade -y \
    && apt install -y \
         clang-7 \
         cmake \
         git \
         libc++-7-dev \
         libc++abi-7-dev \
         libprotobuf-dev \
         libssl-dev \
         make \
    && apt autoclean

ENV CXX=/usr/bin/clang++-7
ENV CXXFLAGS=-stdlib=libc++
