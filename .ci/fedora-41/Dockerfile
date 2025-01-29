FROM fedora:41

COPY ./robot/dependencies.txt .

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

RUN python3 -m pip install -r dependencies.txt
