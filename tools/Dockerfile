FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Toolchain base e dipendenze
RUN apt update && apt install -y \
  build-essential \
  nasm \
  xorriso \
  mtools \
  parted \
  dosfstools \
  gdisk \
  qemu-system-x86 \
  wget \
  make \
  gcc \
  clang \
  lld \
  binutils \
  git \
  sudo \
  udev \
  software-properties-common

# Clona e builda limine dal branch v9.x-binary
RUN git clone --depth=1 --branch=v9.x-binary https://github.com/limine-bootloader/limine.git /opt/limine && \
  make -C /opt/limine && \
  make -C /opt/limine install

ENV PATH="/opt/limine:/usr/local/bin:/usr/bin:/bin"
ENV PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/limine"

WORKDIR /workspace
