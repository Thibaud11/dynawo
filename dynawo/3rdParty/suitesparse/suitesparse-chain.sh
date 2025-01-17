#!/bin/bash
#
# Copyright (c) 2015-2019, RTE (http://www.rte-france.com)
# See AUTHORS.txt
# All rights reserved.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, you can obtain one at http://mozilla.org/MPL/2.0/.
# SPDX-License-Identifier: MPL-2.0
#
# This file is part of Dynawo, an hybrid C++/Modelica open source time domain
# simulation tool for power systems.
#

error_exit() {
  echo "${1:-"Unknown Error"}" 1>&2
  exit -1
}

export_var_env() {
  local var="$@"
  local name=${var%%=*}
  local value="${var#*=}"

  if eval "[ \"\$$name\" ]"; then
    eval "value=\${$name}"
    return
  fi

  if [ "$value" = UNDEFINED ]; then
    error_exit "You must define the value of $name"
  fi
  export $name="$value"
}

get_absolute_path() {
  python -c "import os; print(os.path.realpath('$1'))"
}

SUITE_SPARSE_VERSION=4.5.4
SUITE_SPARSE_ARCHIVE=SuiteSparse-${SUITE_SPARSE_VERSION}.tar.gz
SUITE_SPARSE_DIRECTORY=SuiteSparse
export_var_env DYNAWO_SUITE_SPARSE_DOWNLOAD_URL=http://faculty.cse.tamu.edu/davis/SuiteSparse

HERE=$PWD

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR=$HERE
INSTALL_DIR=$HERE/install
BUILD_TYPE=Debug
export_var_env DYNAWO_C_COMPILER=$(command -v gcc)
export_var_env DYNAWO_NB_PROCESSORS_USED=1

export_var_env DYNAWO_LIBRARY_TYPE=SHARED

download_suitesparse() {
  cd $SCRIPT_DIR
  if [ ! -f "${SUITE_SPARSE_ARCHIVE}" ]; then
    if [ -x "$(command -v wget)" ]; then
      wget --timeout 10 --tries 3 ${DYNAWO_SUITE_SPARSE_DOWNLOAD_URL}/${SUITE_SPARSE_ARCHIVE} || error_exit "Error while downloading SuiteSparse."
    elif [ -x "$(command -v curl)" ]; then
      curl --connect-timeout 10 --retry 2 ${DYNAWO_SUITE_SPARSE_DOWNLOAD_URL}/${SUITE_SPARSE_ARCHIVE} --output ${SUITE_SPARSE_ARCHIVE} || error_exit "Error while downloading SuiteSparse."
    else
      error_exit "You need to install either wget or curl."
    fi
  fi
}

echo_error_exit() {
  echo "#!/bin/bash
error_exit() {
  echo \"\${1:-\"Unknown Error\"}\" 1>&2
  exit -1
}" > $BUILD_DIR/compile_suitesparse_dynawo.sh
}

install_suitesparse() {
  cd $SCRIPT_DIR
  if [ ! -d "$BUILD_DIR/$SUITE_SPARSE_DIRECTORY" ]; then
    tar -xzf $SUITE_SPARSE_ARCHIVE -C $BUILD_DIR
  fi
  if [ "${BUILD_TYPE}" = "Debug" ]; then
    CC_FLAG="-g"
  else
    CC_FLAG=""
  fi
  if [ "`uname`" = "Darwin" ]; then
    CC_FLAG="$CC_FLAG -isysroot $(xcrun --show-sdk-path)"
    if [ ! -z "$MACOSX_DEPLOYMENT_TARGET" ]; then
      CC_FLAG="$CC_FLAG -mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET"
    fi
  fi
  echo_error_exit
  if [ "${BUILD_TYPE}" = "Debug" ]; then
    echo 'export OPTIMIZATION="-O0"' >> $BUILD_DIR/compile_suitesparse_dynawo.sh
  fi
  if [ "$DYNAWO_LIBRARY_TYPE" = "SHARED" ]; then
    echo "cd $BUILD_DIR/$SUITE_SPARSE_DIRECTORY/SuiteSparse_config
{ make -j $DYNAWO_NB_PROCESSORS_USED CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" library && make CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" INSTALL_LIB=$INSTALL_DIR/lib INSTALL_INCLUDE=$INSTALL_DIR/include install; } || error_exit \"Error while building SuiteSparse\"
if [ \"\`uname\`\" = "Darwin" ]; then install_name_tool -id @rpath/libsuitesparseconfig.dylib $INSTALL_DIR/lib/libsuitesparseconfig.dylib; fi
cd $BUILD_DIR/$SUITE_SPARSE_DIRECTORY/AMD
{ make -j $DYNAWO_NB_PROCESSORS_USED CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" library && make CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" INSTALL_LIB=$INSTALL_DIR/lib INSTALL_INCLUDE=$INSTALL_DIR/include install; } || error_exit \"Error while building AMD\"
if [ \"\`uname\`\" = "Darwin" ]; then install_name_tool -id @rpath/libamd.dylib $INSTALL_DIR/lib/libamd.dylib; fi
cd $BUILD_DIR/$SUITE_SPARSE_DIRECTORY/COLAMD
{ make -j $DYNAWO_NB_PROCESSORS_USED CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" library && make CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" INSTALL_LIB=$INSTALL_DIR/lib INSTALL_INCLUDE=$INSTALL_DIR/include install; } || error_exit \"Error while building COLAMD\"
if [ \"\`uname\`\" = "Darwin" ]; then install_name_tool -id @rpath/libcolamd.dylib $INSTALL_DIR/lib/libcolamd.dylib; fi
cd $BUILD_DIR/$SUITE_SPARSE_DIRECTORY/BTF
{ make -j $DYNAWO_NB_PROCESSORS_USED CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" library && make CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" INSTALL_LIB=$INSTALL_DIR/lib INSTALL_INCLUDE=$INSTALL_DIR/include install; } || error_exit \"Error while building BTF\"
if [ \"\`uname\`\" = "Darwin" ]; then install_name_tool -id @rpath/libbtf.dylib $INSTALL_DIR/lib/libbtf.dylib; fi
cd $BUILD_DIR/$SUITE_SPARSE_DIRECTORY/KLU
{ make -j $DYNAWO_NB_PROCESSORS_USED CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" library && make CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" INSTALL_LIB=$INSTALL_DIR/lib INSTALL_INCLUDE=$INSTALL_DIR/include install; } || error_exit \"Error while building KLU\"
if [ \"\`uname\`\" = "Darwin" ]; then install_name_tool -id @rpath/libklu.dylib $INSTALL_DIR/lib/libklu.dylib; fi" >> $BUILD_DIR/compile_suitesparse_dynawo.sh
  else
    echo "mkdir -p $INSTALL_DIR/lib
mkdir -p $INSTALL_DIR/include
cd $BUILD_DIR/$SUITE_SPARSE_DIRECTORY/SuiteSparse_config
make -j $DYNAWO_NB_PROCESSORS_USED CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" static || error_exit \"Error while building SuiteSparse\"
cp *.a $INSTALL_DIR/lib/ || error_exit \"Error while building SuiteSparse\"
cp SuiteSparse_config.h $INSTALL_DIR/include/ || error_exit \"Error while building SuiteSparse\"
cd $BUILD_DIR/$SUITE_SPARSE_DIRECTORY/AMD
make -j $DYNAWO_NB_PROCESSORS_USED CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" static || error_exit \"Error while building AMD\"
cp Lib/*.a $INSTALL_DIR/lib || error_exit \"Error while building AMD\"
cp Include/amd.h $INSTALL_DIR/include || error_exit \"Error while building AMD\"
cd $BUILD_DIR/$SUITE_SPARSE_DIRECTORY/COLAMD
make -j $DYNAWO_NB_PROCESSORS_USED CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" static || error_exit \"Error while building COLAMD\"
cp Lib/*.a $INSTALL_DIR/lib || error_exit \"Error while building COLAMD\"
cp Include/colamd.h $INSTALL_DIR/include || error_exit \"Error while building COLAMD\"
cd $BUILD_DIR/$SUITE_SPARSE_DIRECTORY/BTF
make -j $DYNAWO_NB_PROCESSORS_USED CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" static || error_exit \"Error while building BTF\"
cp Lib/*.a $INSTALL_DIR/lib || error_exit \"Error while building BTF\"
cp Include/btf.h $INSTALL_DIR/include || error_exit \"Error while building BTF\"
cd $BUILD_DIR/$SUITE_SPARSE_DIRECTORY/KLU
make -j $DYNAWO_NB_PROCESSORS_USED CC=\"$DYNAWO_C_COMPILER $CC_FLAG\" static || error_exit \"Error while building KLU\"
cp Lib/*.a $INSTALL_DIR/lib || error_exit \"Error while building KLU\"
cp Include/klu.h $INSTALL_DIR/include || error_exit \"Error while building KLU\"" >> $BUILD_DIR/compile_suitesparse_dynawo.sh
  fi
  chmod +x $BUILD_DIR/compile_suitesparse_dynawo.sh
  $BUILD_DIR/compile_suitesparse_dynawo.sh
}

while (($#)); do
  case $1 in
    --install-dir=*)
      INSTALL_DIR=$(get_absolute_path `echo $1 | sed -e 's/--install-dir=//g'`)
      if [ ! -d "$INSTALL_DIR" ]; then
        mkdir -p $INSTALL_DIR
      fi
      ;;
    --build-dir=*)
      BUILD_DIR=$(get_absolute_path `echo $1 | sed -e 's/--build-dir=//g'`)
      if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p $BUILD_DIR
      fi
      ;;
    --build-type=*)
      BUILD_TYPE=`echo $1 | sed -e 's/--build-type=//g'`
      ;;
    *)
      break
      ;;
  esac
  shift
done

download_suitesparse || error_exit "Error while downloading SuiteSparse suite"
install_suitesparse || error_exit "Error while building SuiteSparse suite"
