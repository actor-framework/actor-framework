FROM ubuntu:20.04

COPY ./robot/dependencies.txt .

RUN apt update -y \
    && apt upgrade -y \
    && DEBIAN_FRONTEND="noninteractive" apt install -y \
         cmake \
         g++ \
         git \
         libssl-dev \
         make \
         python3 \
         python3-pip \
    && apt-get autoclean

RUN python3 -m pip install -r dependencies.txt
