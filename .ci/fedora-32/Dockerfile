FROM fedora:32

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
    && dnf clean all
