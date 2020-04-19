FROM fedora:30

RUN dnf update -y && \
  dnf clean all

RUN dnf install -y \
  git \
  make \
  cmake \
  gcc-c++ \
  openssl-devel \
  libasan \
  libubsan \
 && dnf clean all
