############################################################
# Dockerfile for Jenkins slave with gcc 7
# Based on Fedora 27
############################################################

FROM fedora:27
MAINTAINER Raphael Hiesgen

# Install Essentials
RUN dnf update -y && \
  yum clean all

##################### INSTALL TOOLS ########################

# Install Packages
RUN dnf install -y git \
  wget \
  sudo \
  java-9-openjdk \
  cmake \
  gcc gcc-c++ \
  clang \
  openssl-devel \
  libasan \
  libubsan \
  gcovr \
 && dnf clean all

####################### SETUP SSH #########################

# Create Jenkins User
RUN useradd jenkins -m -s /bin/bash

# Setup home for Jenkins
RUN chown -R jenkins /home/jenkins
RUN chgrp -R jenkins /home/jenkins

# Add the jenkins user to sudoers
# RUN echo "jenkins  ALL=(ALL)  ALL" >> etc/sudoers

# Set Name Servers
# COPY /files/resolv.conf /etc/resolv.conf

# Expose SSH port and run SSHD
EXPOSE 22
CMD ["/usr/sbin/sshd","-D"]
