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
  exit 1
}

export_var_env() {
  var=$@
  name=${var%%=*}
  value=${var#*=}

  if eval "[ \$$name ]"; then
    eval "value=\${$name}"
    ##echo "Environment variable for $name already set : $value"
    return
  fi

  if [ "$value" = UNDEFINED ]; then
    error_exit "You must define the value of $name"
  fi
  export $name="$value"
}

check_git_version() {
  if [ -x "$(command -v git)" ]; then
    GIT_VERSION=$(git --version | grep -o "[0-9][.].*")
    if [ $(echo $GIT_VERSION | cut -d '.' -f 1) -ge 2 ]; then
      if [ $(echo $GIT_VERSION | cut -d '.' -f 2) -ge 11 ]; then
        return 0
      fi
    elif [ $(echo $GIT_VERSION | cut -d '.' -f 1) -gt 2 ]; then
      return 0
    fi
  else
    error_exit "You need to install git command line utility."
  fi
  return 1
}

# Default values
SRC_OPENMODELICA=""
OPENMODELICA_VERSION=""
MODELICA_LIB=""
export_var_env DYNAWO_OPENMODELICA_GIT_URL=https://openmodelica.org/git-readonly/OpenModelica.git
export_var_env DYNAWO_MODELICA_GIT_URL=https://github.com/modelica/ModelicaStandardLibrary.git

while (($#)); do
  case $1 in
    --openmodelica-dir=*)
      SRC_OPENMODELICA=`echo $1 | sed -e 's/--openmodelica-dir=//g'`
      ;;
    --openmodelica-version=*)
      OPENMODELICA_VERSION=`echo $1 | sed -e 's/--openmodelica-version=//g'`
      ;;
    --modelica-version=*)
      MODELICA_LIB=`echo $1 | sed -e 's/--modelica-version=//g'`
      ;;
    *)
      break
      ;;
  esac
  shift
done

check_configuration() {
  if [ -z "$SRC_OPENMODELICA" ]; then
    error_exit "You need to give a source directory for OpenModelica with --openmodelica-dir option."
  fi
  if [ -z "$OPENMODELICA_VERSION" ]; then
    error_exit "You need to give a version of OpenModelica to checkout with --openmodelica-version option, for example 1.9.3."
  fi
  if [ -z "$MODELICA_LIB" ]; then
    error_exit "You need to give a Modelica Library version with --modelica-version option, for example 3.2.2."
  fi
}

check_tag_openmodelica() {
  if [ -d "$SRC_OPENMODELICA" ]; then
    cd $SRC_OPENMODELICA
    last_log_openmodelica=$(git log -1 --decorate | grep -o "tag: v${OPENMODELICA_VERSION//_/.}")
    if [ "$last_log_openmodelica" != "tag: v${OPENMODELICA_VERSION//_/.}" ]; then
      return 1
    fi
  else
    error_exit "$SRC_OPENMODELICA folder does not exist."
  fi
}

check_tag_omcompiler() {
  if [ -d "$SRC_OPENMODELICA/OMCompiler" ]; then
    cd $SRC_OPENMODELICA/OMCompiler
    last_log_omcompiler=$(git log -1 --decorate | grep -o "tag: v${OPENMODELICA_VERSION//_/.}")
    if [ "$last_log_omcompiler" != "tag: v${OPENMODELICA_VERSION//_/.}" ]; then
      return 1
    fi
  else
    error_exit "$SRC_OPENMODELICA/OMCompiler folder does not exist."
  fi
}

check_tag_modelica_library() {
  if [ -d "$SRC_OPENMODELICA/libraries/Modelica" ]; then
    cd $SRC_OPENMODELICA/libraries/Modelica
    last_log_modelica_lib=$(git log -1 --decorate | grep -o "v${MODELICA_LIB//_/.}")
    if [ "$last_log_modelica_lib" != "v${MODELICA_LIB//_/.}" ]; then
      return 1
    fi
  else
    error_exit "$SRC_OPENMODELICA/libraries/Modelica folder does not exist."
  fi
}

check_tags() {
  check_tag_openmodelica || return 1
  check_tag_omcompiler || return 1
  check_tag_modelica_library || return 1
}

checkout_openmodelica_repository() {
  CHECKOUT_FORCE=""
  if ! check_git_version; then
    CHECKOUT_FORCE="-f"
  fi
  if [ ! -d "$SRC_OPENMODELICA" ]; then
    git clone $DYNAWO_OPENMODELICA_GIT_URL $SRC_OPENMODELICA || error_exit "Git clone of OpenModelica in $SRC_OPENMODELICA failed."
    if [ -d "$SRC_OPENMODELICA" ]; then
      cd "$SRC_OPENMODELICA"
      git checkout $CHECKOUT_FORCE tags/v${OPENMODELICA_VERSION//_/.} || error_exit "Git checkout tags/v${OPENMODELICA_VERSION//_/.} failed for OpenModelica in $SRC_OPENMODELICA."
      GIT_OPTION=""
      if check_git_version; then
        GIT_OPTION="--progress"
      fi
      git submodule update --init $GIT_OPTION OMCompiler || error_exit "Git clone of OMCompiler in $SRC_OPENMODELICA failed."
      (cd OMCompiler; git submodule update --init $GIT_OPTION 3rdParty;) || error_exit "Git clone of OMCompiler/3rdParty in $SRC_OPENMODELICA failed."
      (cd OMCompiler; git submodule update --init $GIT_OPTION common;) || error_exit "Git clone of OMCompiler/common in $SRC_OPENMODELICA failed."
      git submodule update --init $GIT_OPTION --recursive libraries || error_exit "Git clone of libraries in $SRC_OPENMODELICA failed."
      git submodule update --init $GIT_OPTION --recursive common || error_exit "Git clone of common in $SRC_OPENMODELICA failed."
      if [ -d "$SRC_OPENMODELICA/libraries" ]; then
        cd libraries && git clone $DYNAWO_MODELICA_GIT_URL Modelica || error_exit "Git clone of Modelica Standard Library failed in $SRC_OPENMODELICA/libraries."
      fi
    fi
  fi
  # Cannot do an if/else here otherwise the first time the repository would not be checked-out well.
  if [ -d "$SRC_OPENMODELICA" ]; then
    check_tags
    RETURN_CODE=$?
    if [[ "$RETURN_CODE" != 0 ]]; then
      cd $SRC_OPENMODELICA
      git checkout $CHECKOUT_FORCE tags/v${OPENMODELICA_VERSION//_/.} || error_exit "Git checkout tags/v${OPENMODELICA_VERSION//_/.} failed for OpenModelica in $SRC_OPENMODELICA."
      if [ -d "$SRC_OPENMODELICA/OMCompiler" ]; then
        pushd OMCompiler && git checkout tags/v${OPENMODELICA_VERSION//_/.} && popd || error_exit "Git checkout tags/v${OPENMODELICA_VERSION//_/.} failed for OMCompiler in $SRC_OPENMODELICA/OMCompiler."
      fi
      if [ -d "$SRC_OPENMODELICA/libraries/Modelica" ]; then
        pushd libraries/Modelica && git checkout tags/v${MODELICA_LIB//_/.} && popd || error_exit "Git checkout tags/v${MODELICA_LIB//_/.} failed for Modelica Standard Library in $SRC_OPENMODELICA/libraries/Modelica."
      fi
    fi
    check_tag_openmodelica || error_exit "OpenModelica needs to be in version ${OPENMODELICA_VERSION//_/.}."
    check_tag_omcompiler || error_exit "OpenModelica Compiler needs to be in version ${OPENMODELICA_VERSION//_/.}."
    check_tag_modelica_library || error_exit "Modelica Standard Library needs to be in version ${MODELICA_LIB//_/.}."
    echo "OpenModelica source folder is configured with the right version."
  fi
}

check_configuration
checkout_openmodelica_repository
