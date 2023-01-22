FROM centos:7

RUN yum update -y \
    && yum install -y centos-release-scl \
    && yum install -y epel-release \
    && yum update -y \
    && yum install -y \
         cmake3 \
         devtoolset-8 \
         devtoolset-8-libasan-devel \
         devtoolset-8-libubsan-devel \
         git \
         make \
         openssl-devel \
    && yum clean all

ENV CXX=/opt/rh/devtoolset-8/root/usr/bin/g++
