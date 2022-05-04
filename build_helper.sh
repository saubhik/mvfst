#!/bin/bash -eu

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# This is a helpful script to build MVFST in the supplied dir
# It pulls in dependencies such as folly and fizz in the _build/deps dir.

# Obtain the mvfst repository root folder at the very start
MVFST_ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

# Useful constants
COLOR_RED="\033[0;31m"
COLOR_GREEN="\033[0;32m"
COLOR_OFF="\033[0m"

usage() {
  cat 1>&2 <<EOF

Usage ${0##*/} [-h|?] [-p PATH]
  -p BUILD_DIR                           (optional): Path of the base dir for mvfst
  -m                                     (optional): Build folly without jemalloc
  -s                                     (optional): Skip installing system package dependencies
  -c                                     (optional): Use ccache
  -z                                     (optional): enable CCP support
  -f                                     (optional): Skip fetching dependencies (to test local changes)
  -h|?                                               Show this help message
EOF
}

FETCH_DEPENDENCIES=true
while getopts ":hp:msczf" arg; do
  case $arg in
  p)
    BUILD_DIR="${OPTARG}"
    ;;
  m)
    MVFST_FOLLY_USE_JEMALLOC="n"
    ;;
  s)
    MVFST_SKIP_SYSTEM_DEPENDENCIES=true
    ;;
  c)
    MVFST_USE_CCACHE=true
    ;;
  z)
    MVFST_ENABLE_CCP=true
    ;;
  f)
    FETCH_DEPENDENCIES=false
    ;;
  h | *) # Display help.
    usage
    exit 0
    ;;
  esac
done

# Validate required parameters
if [ -z "${BUILD_DIR-}" ]; then
  echo -e "${COLOR_RED}[ INFO ] Build dir is not set. So going to build into _build ${COLOR_OFF}"
  BUILD_DIR=_build
  mkdir -p $BUILD_DIR
fi

if [[ -n "${MVFST_FOLLY_USE_JEMALLOC-}" ]]; then
  if [[ "$MVFST_FOLLY_USE_JEMALLOC" != "n" ]]; then
    unset $MVFST_FOLLY_USE_JEMALLOC
  fi
fi

### Configure necessary build and install directories

cd $BUILD_DIR || exit
BWD=$(pwd)
DEPS_DIR=$BWD/deps
mkdir -p "$DEPS_DIR"

MVFST_BUILD_DIR=$BWD/build
mkdir -p "$MVFST_BUILD_DIR"

INSTALL_PREFIX=/usr/local

FMT_INSTALL_DIR=$INSTALL_PREFIX
GTEST_INSTALL_DIR=$INSTALL_PREFIX
ZSTD_INSTALL_DIR=$INSTALL_PREFIX
FOLLY_INSTALL_DIR=$INSTALL_PREFIX
FIZZ_INSTALL_DIR=$INSTALL_PREFIX
LIBCCP_INSTALL_DIR=$INSTALL_PREFIX
MVFST_INSTALL_DIR=$INSTALL_PREFIX

CMAKE_EXTRA_ARGS=(${CMAKE_EXTRA_ARGS-})
if [[ ! -z "${MVFST_USE_CCACHE-}" ]]; then
  CCACHE=$(which ccache)
  CMAKE_EXTRA_ARGS+=(-DCMAKE_C_COMPILER_LAUNCHER="${CCACHE}")
  CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_COMPILER_LAUNCHER="${CCACHE}")
fi

if [[ ! -z "${MVFST_FOLLY_USE_JEMALLOC-}" ]]; then
  CMAKE_EXTRA_ARGS+=(-DFOLLY_USE_JEMALLOC=0)
fi

# Make this 0 to not compile profiling code.
CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_FLAGS="-DPROFILING_ENABLED=0")
CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_FLAGS="-std=gnu++1z")
CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_FLAGS="-g")
CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_FLAGS="-D_GNU_SOURCE")
CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_FLAGS="-DNDEBUG")
CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_FLAGS="-O3")
CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_FLAGS="-march=native")
CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_FLAGS="-flto")
CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_FLAGS="-ffast-math")
CMAKE_EXTRA_ARGS+=(-DCMAKE_CXX_FLAGS="-DMLX5")

# Default to parallel build width of 4.
# If we have "nproc", use that to get a better value.
# If not, then intentionally go a bit conservative and
# just use the default of 4 (e.g., some desktop/laptop OSs
# have a tendency to freeze if we actually use all cores).
set +x
nproc=4
if [ -z "$(hash nproc 2>&1)" ]; then
  nproc=$(nproc)
fi
set -x

function install_dependencies_linux() {
  sudo apt-get update
  sudo apt-get install -y \
    g++ \
    cmake \
    m4 \
    libboost-all-dev \
    libevent-dev \
    libdouble-conversion-dev \
    libgoogle-glog-dev \
    libgflags-dev \
    libiberty-dev \
    liblz4-dev \
    liblzma-dev \
    libsnappy-dev \
    make \
    zlib1g-dev \
    binutils-dev \
    libjemalloc-dev \
    libssl-dev \
    pkg-config \
    libsodium-dev
}

function install_dependencies_mac() {
  # install the default dependencies from homebrew
  brew install \
    cmake \
    m4 \
    boost \
    double-conversion \
    gflags \
    glog \
    libevent \
    lz4 \
    snappy \
    xz \
    openssl \
    libsodium

  brew link \
    boost \
    double-conversion \
    gflags \
    glog \
    libevent \
    lz4 \
    snappy \
    xz \
    libsodium
}

function setup_fmt() {
  FMT_DIR=$DEPS_DIR/fmt
  FMT_BUILD_DIR=$DEPS_DIR/fmt/build/

  if [ ! -d "$FMT_DIR" ]; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning fmt repo ${COLOR_OFF}"
    git clone https://github.com/fmtlib/fmt.git "$FMT_DIR"
  fi
  cd "$FMT_DIR"
  git fetch --tags
  git checkout 6.2.1
  echo -e "${COLOR_GREEN}Building fmt ${COLOR_OFF}"
  mkdir -p "$FMT_BUILD_DIR"
  cd "$FMT_BUILD_DIR" || exit

  cmake \
    -DCMAKE_PREFIX_PATH="$FMT_INSTALL_DIR" \
    -DCMAKE_INSTALL_PREFIX="$FMT_INSTALL_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DFMT_DOC=OFF \
    -DFMT_TEST=OFF \
    ${CMAKE_EXTRA_ARGS[@]+"${CMAKE_EXTRA_ARGS[@]}"} \
    ..
  make -j "$nproc"
  make install
  echo -e "${COLOR_GREEN}fmt is installed ${COLOR_OFF}"
  cd "$BWD" || exit
}

function setup_googletest() {
  GTEST_DIR=$DEPS_DIR/googletest
  GTEST_BUILD_DIR=$DEPS_DIR/googletest/build/

  if [ ! -d "$GTEST_DIR" ]; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning googletest repo ${COLOR_OFF}"
    git clone https://github.com/google/googletest.git "$GTEST_DIR"
  fi
  cd "$GTEST_DIR"
  git fetch --tags
  git checkout release-1.8.0
  echo -e "${COLOR_GREEN}Building googletest ${COLOR_OFF}"
  mkdir -p "$GTEST_BUILD_DIR"
  cd "$GTEST_BUILD_DIR" || exit

  cmake \
    -DCMAKE_INSTALL_PREFIX="$GTEST_INSTALL_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    ..
  make -j "$nproc"
  make install
  echo -e "${COLOR_GREEN}googletest is installed ${COLOR_OFF}"
  cd "$BWD" || exit
}

function synch_dependency_to_commit() {
  # Utility function to synch a dependency to a specific commit. Takes two arguments:
  #   - $1: folder of the dependency's git repository
  #   - $2: path to the text file containing the desired commit hash
  if [ "$FETCH_DEPENDENCIES" = false ]; then
    return
  fi
  DEP_REV=$(sed 's/Subproject commit //' "$2")
  pushd "$1"
  git fetch
  # Disable git warning about detached head when checking out a specific commit.
  git -c advice.detachedHead=false checkout "$DEP_REV"
  popd
}

function setup_zstd() {
  ZSTD_DIR=$DEPS_DIR/zstd
  ZSTD_BUILD_DIR=$DEPS_DIR/zstd/build/cmake/builddir

  if [ ! -d "$ZSTD_DIR" ]; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning zstd repo ${COLOR_OFF}"
    git clone https://github.com/facebook/zstd.git "$ZSTD_DIR"
  fi

  echo -e "${COLOR_GREEN}Building Zstd ${COLOR_OFF}"
  mkdir -p "$ZSTD_BUILD_DIR"
  cd "$ZSTD_BUILD_DIR" || exit
  cmake -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DCMAKE_PREFIX_PATH="$ZSTD_INSTALL_DIR" \
    -DCMAKE_INSTALL_PREFIX="$ZSTD_INSTALL_DIR" \
    ${CMAKE_EXTRA_ARGS[@]+"${CMAKE_EXTRA_ARGS[@]}"} \
    ..
  make -j "$nproc"
  make install
  echo -e "${COLOR_GREEN}Zstd is installed ${COLOR_OFF}"
  cd "$BWD" || exit
}

function setup_caladan_runtime() {
  CALADAN_DIR=$DEPS_DIR/caladan
  CALADAN_KSCHED_DIR=$DEPS_DIR/caladan/ksched
  CALADAN_BINDINGS_DIR=$DEPS_DIR/caladan/bindings/cc
  CALADAN_CONFIG_DIR=/etc/caladan
  CALADAN_INCLUDE_DIR=/usr/local/include/caladan
  SETUP_MACHINE_SCRIPT=$DEPS_DIR/caladan/scripts/setup_machine.sh

  if [ ! -d "$CALADAN_DIR" ]; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning caladan (from @saubhik) ${COLOR_OFF}"
    git clone -b feature/move-decryption-iokernel https://github.com/saubhik/caladan.git "$CALADAN_DIR"
    echo -e "${COLOR_GREEN}[ INFO ] Building submodules ${COLOR_OFF}"
    cd "$CALADAN_DIR" || exit
    make submodules
  fi

  echo -e "${COLOR_GREEN}Building Caladan runtime ${COLOR_OFF}"

  cd "$CALADAN_DIR" || exit
  make -j "$nproc" libs
  cp -R "$CALADAN_DIR/inc/." "/usr/local/include"

  cd "$CALADAN_KSCHED_DIR" || exit
  make -j "$nproc"

  cd "$CALADAN_DIR" || exit
  sudo "$SETUP_MACHINE_SCRIPT"

  cd "$CALADAN_BINDINGS_DIR" || exit
  make -j "$nproc"

  mkdir -p $CALADAN_INCLUDE_DIR || exit
  cp "$CALADAN_BINDINGS_DIR/runtime.h" "$CALADAN_INCLUDE_DIR/runtime.h"
  cp "$CALADAN_BINDINGS_DIR/net.h" "$CALADAN_INCLUDE_DIR/net.h"
  cp "$CALADAN_BINDINGS_DIR/sh_event.h" "$CALADAN_INCLUDE_DIR/sh_event.h"
  cp "$CALADAN_BINDINGS_DIR/thread.h" "$CALADAN_INCLUDE_DIR/thread.h"
  cp "$CALADAN_BINDINGS_DIR/sync.h" "$CALADAN_INCLUDE_DIR/sync.h"
  cp "$CALADAN_BINDINGS_DIR/timer.h" "$CALADAN_INCLUDE_DIR/timer.h"

  cp "$CALADAN_BINDINGS_DIR/librt++.a" "/usr/local/lib/librt++.a"

  cp "$CALADAN_DIR/libruntime.a" "/usr/local/lib/libruntime.a"
  cp "$CALADAN_DIR/libnet.a" "/usr/local/lib/libnet.a"
  cp "$CALADAN_DIR/libbase.a" "/usr/local/lib/libbase.a"

  mkdir -p "$CALADAN_CONFIG_DIR"
  cp "$CALADAN_DIR/server.config" "$CALADAN_CONFIG_DIR/server.config"
  cp "$CALADAN_DIR/client.config" "$CALADAN_CONFIG_DIR/client.config"
  cp "$CALADAN_DIR/test.config" "$CALADAN_CONFIG_DIR/test.config"

  echo -e "${COLOR_GREEN}Caladan runtime is installed ${COLOR_OFF}"
  cd "$BWD" || exit
}

function setup_caladan_iokernel() {
  CALADAN_DIR=$DEPS_DIR/caladan

  if [ ! -d "$CALADAN_DIR" ]; then
    setup_caladan_runtime
  fi

  echo -e "${COLOR_GREEN}Building Caladan iokerneld ${COLOR_OFF}"

  cd "${CALADAN_DIR}" || exit
  make -j "$nproc" iokerneld

  cp "$CALADAN_DIR/iokerneld" "$BWD/iokerneld"

  echo -e "${COLOR_GREEN}Caladan iokerneld is installed ${COLOR_OFF}"
  cd "$BWD" || exit
}

function setup_folly() {
  FOLLY_DIR=$DEPS_DIR/folly
  CALADAN_DIR=$DEPS_DIR/caladan
  FOLLY_BUILD_DIR=$DEPS_DIR/folly/build/

  if [ ! -d "$FOLLY_DIR" ]; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning folly (from @saubhik) ${COLOR_OFF}"
    git clone -b feature/move-decryption-iokernel https://github.com/saubhik/folly.git "$FOLLY_DIR"
    if [[ -z "${MVFST_SKIP_SYSTEM_DEPENDENCIES-}" ]]; then
      echo -e "${COLOR_GREEN}[ INFO ] install dependencies ${COLOR_OFF}"
      if [ "$Platform" = "Linux" ]; then
        install_dependencies_linux
      elif [ "$Platform" = "Mac" ]; then
        install_dependencies_mac
      else
        echo -e "${COLOR_RED}[ ERROR ] Unknown platform: $Platform ${COLOR_OFF}"
        exit 1
      fi
    else
      echo -e "${COLOR_GREEN}[ INFO ] Skipping installing dependencies ${COLOR_OFF}"
    fi
  fi

  # synch_dependency_to_commit "$FOLLY_DIR" "$MVFST_ROOT_DIR/build/deps/github_hashes/facebook/folly-rev.txt"

  if [ "$Platform" = "Mac" ]; then
    # Homebrew installs OpenSSL in a non-default location on MacOS >= Mojave
    # 10.14 because MacOS has its own SSL implementation.  If we find the
    # typical Homebrew OpenSSL dir, load OPENSSL_ROOT_DIR so that cmake
    # will find the Homebrew version.
    dir=/usr/local/opt/openssl
    if [ -d $dir ]; then
      export OPENSSL_ROOT_DIR=$dir
    fi
  fi

  echo -e "${COLOR_GREEN}Building Folly ${COLOR_OFF}"
  mkdir -p "$FOLLY_BUILD_DIR"
  cd "$FOLLY_BUILD_DIR" || exit

  # check for environment variable. If
  cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$FOLLY_INSTALL_DIR" \
    -DCMAKE_INSTALL_PREFIX="$FOLLY_INSTALL_DIR" \
    -DCMAKE_EXE_LINKER_FLAGS="-T $CALADAN_DIR/base/base.ld" \
    ${CMAKE_EXTRA_ARGS[@]+"${CMAKE_EXTRA_ARGS[@]}"} \
    ..
  make -j "$nproc"
  make install
  echo -e "${COLOR_GREEN}Folly is installed ${COLOR_OFF}"
  cd "$BWD" || exit
}

function setup_fizz() {
  FIZZ_DIR=$DEPS_DIR/fizz
  CALADAN_DIR=$DEPS_DIR/caladan
  FIZZ_BUILD_DIR=$DEPS_DIR/fizz/build/
  if [ ! -d "$FIZZ_DIR" ]; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning fizz (from @saubhik) ${COLOR_OFF}"
    git clone -b feature/add-shenango-support https://github.com/saubhik/fizz "$FIZZ_DIR"
  fi

  # synch_dependency_to_commit "$FIZZ_DIR" "$MVFST_ROOT_DIR/build/deps/github_hashes/facebookincubator/fizz-rev.txt"

  echo -e "${COLOR_GREEN}Building Fizz ${COLOR_OFF}"
  mkdir -p "$FIZZ_BUILD_DIR"
  cd "$FIZZ_BUILD_DIR" || exit
  cmake -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DCMAKE_PREFIX_PATH="$FIZZ_INSTALL_DIR" \
    -DCMAKE_INSTALL_PREFIX="$FIZZ_INSTALL_DIR" \
    -DCMAKE_EXE_LINKER_FLAGS="-T $CALADAN_DIR/base/base.ld" \
    ${CMAKE_EXTRA_ARGS[@]+"${CMAKE_EXTRA_ARGS[@]}"} \
    "$FIZZ_DIR/fizz"
  make -j "$nproc"
  make install
  echo -e "${COLOR_GREEN}Fizz is installed ${COLOR_OFF}"
  cd "$BWD" || exit
}

function detect_platform() {
  unameOut="$(uname -s)"
  case "${unameOut}" in
  Linux*) Platform=Linux ;;
  Darwin*) Platform=Mac ;;
  *) Platform="UNKNOWN:${unameOut}" ;;
  esac
  echo -e "${COLOR_GREEN}Detected platform: $Platform ${COLOR_OFF}"
}

function setup_libccp() {
  LIBCCP_DIR=$DEPS_DIR/libccp
  LIBCCP_BUILD_DIR=$LIBCCP_DIR/build/
  if [ ! -d "$LIBCCP_DIR" ]; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning libccp repo ${COLOR_OFF}"
    git clone https://github.com/ccp-project/libccp "$LIBCCP_DIR"
  fi

  #synch_dependency_to_commit "$LIBCCP_DIR" "$MVFST_ROOT_DIR/build/deps/github_hashes/libccp-rev.txt"

  echo -e "${COLOR_GREEN}Building libccp ${COLOR_OFF}"
  mkdir -p "$LIBCCP_BUILD_DIR"
  cd "$LIBCCP_BUILD_DIR" || exit
  cmake -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DCMAKE_PREFIX_PATH="$LIBCCP_INSTALL_DIR" \
    -DCMAKE_INSTALL_PREFIX="$LIBCCP_INSTALL_DIR" \
    -DCMAKE_CXX_FLAGS=\D__CPLUSPLUS__=1 \
    ${CMAKE_EXTRA_ARGS[@]+"${CMAKE_EXTRA_ARGS[@]}"} \
    "$LIBCCP_DIR"
  make -j "$nproc"
  make install
  echo -e "${COLOR_GREEN}libccp is installed ${COLOR_OFF}"
  cd "$BWD" || exit
}

function setup_rust() {
  if ! [ -x "$(command -v rustc)" ] || ! [ -x "$(command -v cargo)" ]; then
    echo -e "${COLOR_RED}[ ERROR ] Rust not found (required for CCP support).${COLOR_OFF}\n"
    echo -e "    To install rust, run the following command, then rerun build_helper.sh:\n"
    echo -e "    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y\n"
    echo -e "    You may also need to run \`source $HOME/.cargo/env\` after installing to add rust to your PATH.\n\n"
    exit
  else
    echo -e "${COLOR_GREEN}[ INFO ] Found rust (required for CCP support).${COLOR_OFF}"
  fi
}

detect_platform
setup_fmt
setup_googletest
setup_zstd
setup_caladan_runtime
setup_folly
setup_fizz
if [[ -n "${MVFST_ENABLE_CCP-}" ]]; then
  setup_libccp
  setup_rust
fi

# build mvfst:
cd "$MVFST_BUILD_DIR" || exit
mvfst_cmake_build_args=(
  -DCMAKE_PREFIX_PATH="$FOLLY_INSTALL_DIR"
  -DCMAKE_INSTALL_PREFIX="$MVFST_INSTALL_DIR"
  -DCMAKE_BUILD_TYPE=Release
  -DBUILD_TESTS=On
  ${CMAKE_EXTRA_ARGS[@]+"${CMAKE_EXTRA_ARGS[@]}"}
)
if [[ -n "${MVFST_ENABLE_CCP-}" ]]; then
  mvfst_cmake_build_args+=(-DCCP_ENABLED=TRUE)
fi
cmake "${mvfst_cmake_build_args[@]}" ../..
make -j "$nproc"
make install

setup_caladan_iokernel

echo -e "${COLOR_GREEN}MVFST is installed ${COLOR_OFF}"
