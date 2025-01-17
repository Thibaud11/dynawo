# Copyright (c) 2015-2019, RTE (http://www.rte-france.com)
# See AUTHORS.txt
# All rights reserved.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, you can obtain one at http://mozilla.org/MPL/2.0/.
# SPDX-License-Identifier: MPL-2.0
#
# This file is part of Dynawo, an hybrid C++/Modelica open source time domain simulation tool for power systems.

SET(SOLVER_KINSOL_VERSION_STRING ${DYNAWO_VERSION_STRING})
SET(SOLVER_KINSOL_VERSION_MAJOR ${DYNAWO_VERSION_MAJOR})

SET(SOLVER_KINSOL_SOURCES
  DYNSolverKIN.cpp
)

SET(SOLVER_KINSOL_INCLUDE_HEADERS
  DYNSolverKIN.h
)

ADD_LIBRARY(dynawo_SolverKINSOL ${LIBRARY_TYPE} ${SOLVER_KINSOL_SOURCES})

TARGET_INCLUDE_DIRECTORIES(dynawo_SolverKINSOL
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${INCLUDEDIR_NAME}>
)

TARGET_LINK_LIBRARIES(dynawo_SolverKINSOL
  PUBLIC
    dynawo_SolverCommon
    Sundials::Sundials_SUNLINSOLKLU
    Sundials::Sundials_KINSOL
    SuiteSparse::SuiteSparse_KLU
    SuiteSparse::SuiteSparse_AMD
    SuiteSparse::SuiteSparse_COLAMD
    SuiteSparse::SuiteSparse_BTF
    SuiteSparse::SuiteSparse_Config
)

IF(${NICSLU_FOUND})
  TARGET_LINK_LIBRARIES(dynawo_SolverKINSOL
    PUBLIC
      Sundials::Sundials_SUNLINSOLNICSLU
      Sundials::Sundials_SUNMATRIXSPARSE
      NICSLU::NICSLU
      NICSLU::NICSLU_Util
  )
ENDIF()

SET_TARGET_PROPERTIES(dynawo_SolverKINSOL PROPERTIES VERSION ${SOLVER_KINSOL_VERSION_STRING}
                                                   SOVERSION ${SOLVER_KINSOL_VERSION_MAJOR})

INSTALL(TARGETS dynawo_SolverKINSOL EXPORT dynawo-targets DESTINATION ${LIBDIR_NAME})
INSTALL(FILES ${SOLVER_KINSOL_INCLUDE_HEADERS} DESTINATION ${INCLUDEDIR_NAME})
