FROM ubuntu:18.04

RUN apt update -y \
    && apt upgrade -y \
    && apt install -y \
         cmake \
         g++-8 \
         git \
         libssl-dev \
         make \
    && apt-get autoclean

ENV CXX=/usr/bin/g++-8
