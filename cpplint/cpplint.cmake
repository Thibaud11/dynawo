# Copyright (c) 2015-2019, RTE (http://www.rte-france.com)
# See AUTHORS.txt
# All rights reserved.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, you can obtain one at http://mozilla.org/MPL/2.0/.
# SPDX-License-Identifier: MPL-2.0
#
# This file is part of Dynawo, an hybrid C++/Modelica open source time domain simulation tool for power systems.
set(CPPLINT_PATH ${DYNAWO_HOME}/cpplint/cpplint-wrapper.py)

add_custom_target(cpplint_download ALL
  COMMAND ${PYTHON_EXECUTABLE} -m pip install cpplint -t ${DYNAWO_HOME}/cpplint)

add_custom_target(cpplint ALL
  DEPENDS cpplint_download
  COMMAND ${PYTHON_EXECUTABLE} ${CPPLINT_PATH} --modified ${DYNAWO_HOME})
