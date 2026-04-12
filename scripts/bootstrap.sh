#!/bin/bash

set -exu

export DEBIAN_FRONTEND=noninteractive

echo "Bootstrapping VM for toy-tcpip..."
echo "Run script as non-root user: whoami=$(whoami)"

# install general packages
echo "Installing general packages..."
sudo apt-get update -y && sudo apt-get install -y \
    git \
    make \
    manpages-dev \
    gcc \
    clangd \
    clang-format

set +exu

echo "Bootstrap script completed successfully!"
