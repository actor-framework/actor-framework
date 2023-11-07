FROM fedora:38

ADD . /sources
RUN ls -la
RUN ls -la /sources

RUN dnf update -y \
    && dnf clean all \
    && dnf install -y \
         cmake \
         gcc-c++ \
         git \
         libasan \
         libubsan \
         make \
         openssl-devel \
         python3 \
         python3-pip \
    && dnf clean all

RUN ls -la
RUN ls -la /sources

RUN python3 -m pip install -r /sources/robot/dependencies.txt
