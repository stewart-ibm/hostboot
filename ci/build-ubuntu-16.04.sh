#!/bin/bash
# IBM_PROLOG_BEGIN_TAG
# This is an automatically generated prolog.
#
# $Source: ci/build-ubuntu-16.04.sh $
#
# OpenPOWER HostBoot Project
#
# Contributors Listed Below - COPYRIGHT 2017
#
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#
# IBM_PROLOG_END_TAG

set -uo pipefail
set -e

MAKE_J=`grep -c processor /proc/cpuinfo`

export CROSS="ccache powerpc64le-linux-gnu-"

wget https://buildroot.org/downloads/buildroot-2017.02.2.tar.bz2 
tar xfj buildroot-*.tar.bz2
(cd buildroot-*; cp ../buildroot_defconfig .config; make olddefconfig; make host-binutils)
JAILCMD="" SKIP_BINARY_FILES=1 CONFIG_FILE=./src/build/configs/op_sim.config OPENPOWER_BUILD=1 CROSS_PREFIX=powerpc64le-linux-gnu- HOST_PREFIX="" V=1 HOST_BINUTILS_DIR=`pwd`/../output/build/host-binutils-2.27/ make -j4

