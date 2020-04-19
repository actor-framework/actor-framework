############################################################
# Dockerfile for Jenkins slave with clang-4.0
# Based on Debian Stretch
############################################################

FROM debian:stretch
MAINTAINER Raphael Hiesgen

# Install Essentials
RUN apt-get update -y && \
  apt-get upgrade -y && \
  apt-get autoremove -y && \
  apt-get autoclean -y

##################### INSTALL TOOLS ########################

# Install Packages
RUN apt-get install -y git \
  wget \
  openjdk-8-jdk \
  cmake \
  gcc g++ \
  clang-4.0 \
  libssl-dev \
  gcovr \
 && apt-get autoclean

####################### SETUP SSH #########################

# Create Jenkins User
RUN useradd jenkins -m -s /bin/bash

# set clang as default compiler
RUN echo "export CXX=clang++-4.0" >> /home/jenkins/.bashrc \
   && echo "export CC=clang-4.0" >> /home/jenkins/.bashrc

# Setup home for Jenkins
RUN chown -R jenkins /home/jenkins
RUN chgrp -R jenkins /home/jenkins

# Add the jenkins user to sudoers
# RUN echo "jenkins  ALL=(ALL)  ALL" >> etc/sudoers

# Set Name Servers
# COPY /files/resolv.conf /etc/resolv.conf

# Ubuntu requires this dir to be created
RUN mkdir -p /var/run/sshd

# Expose SSH port and run SSHD
EXPOSE 22
CMD ["/usr/sbin/sshd","-D"]
