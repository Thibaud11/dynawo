//
// Copyright (c) 2015-2019, RTE (http://www.rte-france.com)
// See AUTHORS.txt
// All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, you can obtain one at http://mozilla.org/MPL/2.0/.
// SPDX-License-Identifier: MPL-2.0
//
// This file is part of Dynawo, an hybrid C++/Modelica open source time domain
// simulation tool for power systems.
//

/* Linearization */
/* Simulation code for MACHINE_PQ generated by the OpenModelica Compiler OMCompiler v1.9.4. */

#include "openmodelica.h"
#include "openmodelica_func.h"
#include "simulation_data.h"
#include "simulation/simulation_info_json.h"
#include "simulation/simulation_runtime.h"
#include "util/omc_error.h"
#include "simulation/solver/model_help.h"
#include "simulation/solver/delay.h"
#include "simulation/solver/linearSystem.h"
#include "simulation/solver/nonlinearSystem.h"
#include "simulation/solver/mixedSystem.h"

#include <string.h>

#include "MACHINE_PQ_functions.h"
#include "MACHINE_PQ_model.h"
#include "MACHINE_PQ_literals.h"




#if defined(HPCOM) && !defined(_OPENMP)
  #error "HPCOM requires OpenMP or the results are wrong"
#endif
#if defined(_OPENMP)
  #include <omp.h>
#else
  /* dummy omp defines */
  #define omp_get_max_threads() 1
#endif
#if defined(__cplusplus)
extern "C" {
#endif

const char *MACHINE_PQ_linear_model_frame()
{
  return "model linear_MACHINE__PQ\n  parameter Integer n = 3; // states \n  parameter Integer k = 0; // top-level inputs \n  parameter Integer l = 0; // top-level outputs \n"
  "  parameter Real x0[3] = {%s};\n"
  "  parameter Real u0[0] = {%s};\n"
  "  parameter Real A[3,3] = [%s];\n"
  "  parameter Real B[3,0] = zeros(3,0);%s\n"
  "  parameter Real C[0,3] = zeros(0,3);%s\n"
  "  parameter Real D[0,0] = zeros(0,0);%s\n"
  "  Real x[3](start=x0);\n"
  "  input Real u[0];\n"
  "  output Real y[0];\n"
  "\n  Real x_PMACHINEPomegaRefPuPvalue = x[1];\n  Real x_PMACHINEPterminalPVPim = x[2];\n  Real x_PMACHINEPterminalPVPre = x[3];\n      \n"
  "equation\n  der(x) = A * x + B * u;\n  y = C * x + D * u;\nend linear_MACHINE__PQ;\n";
}
#if defined(__cplusplus)
}
#endif
