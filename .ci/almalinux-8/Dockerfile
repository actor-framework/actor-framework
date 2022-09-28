FROM almalinux:8

RUN dnf update -y \
    && dnf install -y \
         cmake \
         gcc-c++ \
         git \
         libasan \
         libubsan \
         make \
         openssl-devel \
    && dnf clean all
