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

/**
 * @file  DYNSolverIDA.cpp
 *
 * @brief Solver implementation based on sundials/IDA solver
 */

#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <set>
#include <sstream>
#include <vector>
#include <algorithm>
#include <map>
#include <cstring>

#include <boost/shared_ptr.hpp>


#include <ida/ida.h>
#include <nvector/nvector_serial.h>
#include <sundials/sundials_types.h>
#include <sundials/sundials_math.h>
#include <sundials/sundials_sparse.h>
#include <sunlinsol/sunlinsol_klu.h>

#include "PARParametersSet.h"
#include "PARParameter.h"

#include "DYNModel.h"
#include "DYNMacrosMessage.h"
#include "DYNSparseMatrix.h"
#include "DYNSolverIDA.h"
#include "DYNSolverKIN.h"
#include "DYNTrace.h"
#include "DYNTimer.h"
#include "DYNSolverCommon.h"

using std::endl;
using std::make_pair;
using std::map;
using std::max;
using std::min;
using std::ofstream;
using std::ostringstream;
using std::set;
using std::setfill;
using std::setw;
using std::string;
using std::stringstream;
using std::vector;

using boost::shared_ptr;

using parameters::ParametersSet;

/**
 * @brief SolverIDAFactory getter
 * @return A pointer to a new instance of SolverIDAFactory
 */
extern "C" DYN::SolverFactory * getFactory() {
  return (new DYN::SolverIDAFactory());
}

/**
 * @brief SolverIDAFactory destroy method
 */
extern "C" void deleteFactory(DYN::SolverFactory* factory) {
  delete factory;
}

/**
 * @brief SolverIDA getter
 * @return A pointer to a new instance of SolverIDA
 */
extern "C" DYN::Solver* DYN::SolverIDAFactory::create() const {
  DYN::Solver* solver(new DYN::SolverIDA());
  return solver;
}

/**
 * @brief SolverIDA destroy method
 */
extern "C" void DYN::SolverIDAFactory::destroy(DYN::Solver* solver) const {
  delete solver;
}

/**
 * @brief structure use for map compare and sort double in a map
 */
struct mapcomp {
    /**
   * @brief compare two double
   * @param d1 first double to compare
   * @param d2 second double to compare
   * @return @b true is the first double is greater that the second one
   */
  bool operator()(const double& d1, const double& d2) const {
    return d1 > d2;
  }
};

namespace DYN {

#ifdef _DEBUG_
const bool affDebug = false;  ///< variable used to activate debug log
#endif

SolverIDAFactory::SolverIDAFactory() {
}

SolverIDAFactory::~SolverIDAFactory() {
}

SolverIDA::SolverIDA() {
  flagInit_ = false;

  IDAMem_ = NULL;
  M_ = NULL;
  LS_ = NULL;
  // KINSOL solver Init
  //-----------------------
  solverKINNormal_.reset(new SolverKIN());
  solverKINYPrim_.reset(new SolverKIN());

  lastRowVals_ = NULL;
  order_ = 0;
  initStep_ = 0.;
  minStep_ = 0.;
  maxStep_ = 0.;
  absAccuracy_ = 0.;
  relAccuracy_ = 0.;
  flagInit_ = false;
  previousReinit_ = None;
}

void
SolverIDA::clean() {
  if (M_ != NULL) {
    SUNMatDestroy(M_);
    M_ = NULL;
  }
  if (LS_ != NULL) {
    SUNLinSolFree(LS_);
    LS_ = NULL;
  }
  if (IDAMem_ != NULL) {
    IDAFree(&IDAMem_);
    IDAMem_ = NULL;
  }
  if (lastRowVals_ != NULL) {
    free(lastRowVals_);
    lastRowVals_ = NULL;
  }
}

SolverIDA::~SolverIDA() {
  clean();
}

void
SolverIDA::defineParameters() {
  parameters_.insert(make_pair("order", ParameterSolver("order", VAR_TYPE_INT)));
  parameters_.insert(make_pair("initStep", ParameterSolver("initStep", VAR_TYPE_DOUBLE)));
  parameters_.insert(make_pair("minStep", ParameterSolver("minStep", VAR_TYPE_DOUBLE)));
  parameters_.insert(make_pair("maxStep", ParameterSolver("maxStep", VAR_TYPE_DOUBLE)));
  parameters_.insert(make_pair("absAccuracy", ParameterSolver("absAccuracy", VAR_TYPE_DOUBLE)));
  parameters_.insert(make_pair("relAccuracy", ParameterSolver("relAccuracy", VAR_TYPE_DOUBLE)));
}

void
SolverIDA::setSolverParameters() {
  order_ = findParameter("order").getValue<int>();
  initStep_ = findParameter("initStep").getValue<double>();
  minStep_ = findParameter("minStep").getValue<double>();
  maxStep_ = findParameter("maxStep").getValue<double>();
  absAccuracy_ = findParameter("absAccuracy").getValue<double>();
  relAccuracy_ = findParameter("relAccuracy").getValue<double>();
}

std::string
SolverIDA::solverType() const {
  return "IDASolver";
}

void
SolverIDA::init(const shared_ptr<Model> &model, const double & t0, const double & tEnd) {
  // test if there is continuous variable in the simulated problem.
  if (model->sizeY() == 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverIDANoContinuousVars);

  // (1) Arguments
  // -------------
  tSolve_ = t0;

  // (2) Problem sizing
  // -------------------------------
  Solver::Impl::init(t0, model);

  // (4) IDACreate
  // -------------
  // Creation of IDA solver's intern memory space
  IDAMem_ = IDACreate();
  if (IDAMem_ == NULL)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDACreate");

  // (5) IDAInit
  // -----------
  int flag = IDAInit(IDAMem_, evalF, t0, yy_, yp_);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAInit");
#ifdef _DEBUG_
  if (affDebug) {
    Trace::debug() << "-> init.getStartTime() -> " << t0 << Trace::endline;
  }
#endif
  // (6) IDASVtolerances
  // -------------------
  std::vector<double> vYAcc;
  vYAcc.assign(model->sizeY(), absAccuracy_);

  N_Vector yAcc = N_VMake_Serial(model->sizeY(), &(vYAcc[0]));
  if (yAcc == NULL)
    throw DYNError(Error::SUNDIALS_ERROR, SolverCreateAcc);

  flag = IDASVtolerances(IDAMem_, relAccuracy_, yAcc);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASVtolerances");

  if (yAcc != NULL) N_VDestroy_Serial(yAcc);
#ifdef _DEBUG_
  if (affDebug) {
    stringstream ss;
    ss << "-> init.getYacc() -> ";
    for (unsigned int i = 0; i < vYAcc.size(); ++i)
      ss << vYAcc[i] << " ";
    Trace::debug() << ss.str() << Trace::endline;
  }
#endif
  // (7) IDASet
  // ----------

  // initialization of the user's data pointer for IDAResFn
  flag = IDASetUserData(IDAMem_, this);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetUserdata");

  // the solver is given the simulation end time
  flag = IDASetStopTime(IDAMem_, tEnd);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetStopTime");

  // minimum time step
  if (!doubleIsZero(minStep_)) {
    flag = IDASetMinStep(IDAMem_, minStep_);
    if (flag < 0)
      throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetMinStep");
  }

  // maximum time step
  if (!doubleIsZero(maxStep_)) {
    flag = IDASetMaxStep(IDAMem_, maxStep_);
    if (flag < 0)
      throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetMaxStep");
  }

  // sdebug
  flag = IDASetMaxOrd(IDAMem_, order_);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetMaxOrd");

  // no initial time
  if (!doubleIsZero(initStep_)) {
    flag = IDASetInitStep(IDAMem_, initStep_);
    if (flag < 0)
      throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetInitStep");
  }
  flag = IDASetId(IDAMem_, yId_);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetId");

  // algebraic variables are ignored in the error computation (prefered for an order>=2 algorithm)
  flag = IDASetSuppressAlg(IDAMem_, SUNTRUE);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetSuppressAlg");


  // (8) Solver choice
  // -------------------
  M_ = SUNSparseMatrix(model->sizeY(), model->sizeY(), 10., CSR_MAT);
  if (M_ == NULL)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "SUNSparseMatrix");

  /* Create KLU SUNLinearSolver object */
  LS_ = SUNLinSol_KLU(yy_, M_);
  if (LS_ == NULL)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "SUNLinSol_KLU");

  /* Attach the matrix and linear solver */
  flag = IDASetLinearSolver(IDAMem_, LS_, M_);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAKLU");


  // (9) Solver options
  // ---------------------
  flag = IDASetJacFn(IDAMem_, evalJ);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASlsSetSparseJacFn");

  // the solver is given which functions to use to throw error messages
  flag = IDASetErrHandlerFn(IDAMem_, errHandlerFn, NULL);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetErrHandlerFn");

  // (11) IDARootInit
  // ----------------

  // the solver is given which are the discontinuity functions
  flag = IDARootInit(IDAMem_, model_->sizeG(), evalG);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDARootInit");

  // the solver is made not to throw warnings coming from the rootfinding
  flag = IDASetNoInactiveRootWarn(IDAMem_);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetNoInactiveRootWarn");

  Solver::Impl::resetStats();
  g0_.assign(model_->sizeG(), NO_ROOT);
  g1_.assign(model_->sizeG(), NO_ROOT);

#ifdef _DEBUG_
  Trace::debug() << DYNLog(SolverIDAInitOk) << Trace::endline;
#endif

  flag = IDASetStepToleranceIC(IDAMem_, 0.01);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetStepToleranceIC");

  flag = IDASetNonlinConvCoefIC(IDAMem_, 0.0033);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetNonlinConvCoefIC");

  flag = IDASetLineSearchOffIC(IDAMem_, false);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetLineSearchOffIC");

  flag = IDASetMaxNumStepsIC(IDAMem_, 10);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetMaxNumStepIC");

  flag = IDASetMaxNumItersIC(IDAMem_, 20);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetMaxNumItersIC");

  flag = IDASetMaxNumJacsIC(IDAMem_, 100);
  if (flag < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASetMaxNumJacsIC");
}

void
SolverIDA::calculateIC() {
  vector<double> y0;
  y0.assign(vYy_.begin(), vYy_.end());
#ifdef _DEBUG_
  for (int i = 0; i < model_->sizeY(); ++i) {
    Trace::debug() << "Y[" << std::setw(3) << i << "] = "
            << std::setw(15) << vYy_[i]
            << " Yp[" << std::setw(2) << i << "] = "
            << std::setw(15) << vYp_[i]
            << " ID[" << std::setw(2) << i << "] = "
            << std::setw(15) << propertyVar2Str(vYId_[i]) << Trace::endline;
  }
#endif
  // root assessing before init
  // --------------------------------
  vector<double> ySave;
  ySave.assign(vYy_.begin(), vYy_.end());

  // Updating discrete variable values and mode
  model_->evalG(tSolve_, vYy_, vYp_, vYz_, g0_);
  evalZMode(g0_, g1_, tSolve_);

  model_->rotateBuffers();
  state_.reset();
  model_->reinitMode();

  // loops until a stable state is found
  bool change(true);
  int counter = 0;
  solverKINNormal_->init(model_, SolverKIN::KIN_NORMAL, 1e-5, 1e-5, 30, 10, 0, 0, 0);
  do {
    // call to solver KIN in order to find the new (adequate) algebraic variables's values
    solverKINNormal_->setInitialValues(tSolve_, vYy_, vYp_);
    solverKINNormal_->solve();
    solverKINNormal_->getValues(vYy_, vYp_);

    // Reinitialization (forced to start over with a small time step)
    // -------------------------------------------------------
    int flag0 = IDAReInit(IDAMem_, tSolve_, yy_, yp_);
    if (flag0 < 0)
      throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAReinit");
#ifdef _DEBUG_
    Trace::debug() << DYNLog(SolverIDABeforeCalcIC) << Trace::endline;
    for (int i = 0; i < model_->sizeY(); ++i) {
      Trace::debug() << "Y[" << std::setw(3) << i << "] = " << std::setw(15) << vYy_[i] << " Yp[" << std::setw(3) << i << "] = " << std::setw(15) << vYp_[i]
              << " diffY[" << std::setw(3) << i << "] = " << vYy_[i] - ySave[i] << Trace::endline;
    }
#endif
    flagInit_ = true;
    int flag = IDACalcIC(IDAMem_, IDA_YA_YDP_INIT, 10.);
    analyseFlag(flag);

    // gathering of values computed by IDACalcIC
    flag = IDAGetConsistentIC(IDAMem_, yy_, yp_);
    if (flag < 0)
      throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetConsistentIC");
#ifdef _DEBUG_
    Trace::debug() << DYNLog(SolverIDAAfterInit) << Trace::endline;
    double maxDiff = 0;
    int indice = -1;
    for (int i = 0; i < model_->sizeY(); ++i) {
      Trace::debug() << "Y[" << std::setw(3) << i << "] = " << std::setw(15) << vYy_[i] << " Yp[" << std::setw(3) << i << "] = " << std::setw(15) << vYp_[i]
              << " diff =" << std::setw(15) << y0[i] - vYy_[i] << Trace::endline;
      if (std::abs(y0[i] - vYy_[i]) > maxDiff) {
        maxDiff = std::abs(y0[i] - vYy_[i]);
        indice = i;
      }
    }
    Trace::debug() << DYNLog(SolverIDAMaxDiff, maxDiff, indice) << Trace::endline;
#endif
    // Root stabilization
    change = false;
    model_->evalG(tSolve_, vYy_, vYp_, vYz_, g1_);
    bool rootFound = !(std::equal(g0_.begin(), g0_.end(), g1_.begin()));
    if (rootFound) {
      g0_.assign(g1_.begin(), g1_.end());
      change = evalZMode(g0_, g1_, tSolve_);
    }

    model_->rotateBuffers();
    state_.reset();
    model_->reinitMode();

    ++counter;
    if (counter >= 100)
      throw DYNError(Error::SOLVER_ALGO, SolverIDAUnstableRoots);
  } while (change);

  // reinit output
  flagInit_ = false;
  solverKINNormal_->clean();
}

void
SolverIDA::analyseFlag(const int & flag) {
  stringstream msg;
  switch (flag) {
    case IDA_SUCCESS:
      msg << DYNLog(IdaSuccess);
      break;
    case IDA_MEM_NULL:
      msg << DYNLog(IdaMemNull);
      break;
    case IDA_NO_MALLOC:
      msg << DYNLog(IdaNoMalloc);
      break;
    case IDA_ILL_INPUT:
      msg << DYNLog(IdaIllInput);
      break;
    case IDA_LSETUP_FAIL:
      msg << DYNLog(IdalsetupFail);
      break;
    case IDA_LINIT_FAIL:
      msg << DYNLog(IdaLinitFail);
      break;
    case IDA_LSOLVE_FAIL:
      msg << DYNLog(IdaLsolveFail);
      break;
    case IDA_BAD_EWT:
      msg << DYNLog(IdaBadEwt);
      break;
    case IDA_FIRST_RES_FAIL:
      msg << DYNLog(IdaFirstResFail);
      break;
    case IDA_RES_FAIL:
      msg << DYNLog(IdaResFail);
      break;
    case IDA_NO_RECOVERY:
      msg << DYNLog(IdaNoRecovery);
      break;
    case IDA_CONSTR_FAIL:
      msg << DYNLog(IdaConstrFail);
      break;
    case IDA_LINESEARCH_FAIL:
      msg << DYNLog(IdaLinesearchFail);
      break;
    case IDA_CONV_FAIL:
      msg << DYNLog(IdaConvFail);
      break;
    default:
#ifdef _DEBUG_
      Trace::debug() << DYNLog(SolverIDAUnknownError) << Trace::endline;
#endif
      throw DYNError(Error::SUNDIALS_ERROR, SolverIDAError);
  }

  if (flag < 0) {
#ifdef _DEBUG_
    Trace::debug() << msg.str() << Trace::endline;
#endif
    throw DYNError(Error::SUNDIALS_ERROR, SolverIDAError);
  }
}

int
SolverIDA::evalF(realtype tres, N_Vector yy, N_Vector yp,
        N_Vector rr, void *data) {
  Timer timer("SolverIDA::evalF");
  SolverIDA* solv = reinterpret_cast<SolverIDA*> (data);
  shared_ptr<Model> model = solv->getModel();

  realtype *iyy = NV_DATA_S(yy);
  realtype *iyp = NV_DATA_S(yp);
  realtype *irr = NV_DATA_S(rr);
  model->evalF(tres, iyy, iyp, irr);
#ifdef _DEBUG_
  if (solv->flagInit()) {
    Trace::debug() << "===== " << DYNLog(SolverIDADebugResidual) << " =====" << Trace::endline;
    for (int i = 0; i < model->sizeF(); ++i) {
      if (std::abs(irr[i]) > 1e-04) {
        Trace::debug() << "  f[" << i << "]=" << irr[i] << Trace::endline;
      }
    }
  }
#endif
return (0);
}

int
SolverIDA::evalG(realtype tres, N_Vector yy, N_Vector yp, realtype *gout,
        void *data) {
  Timer timer("SolverIDA::evalG");
  SolverIDA* solv = reinterpret_cast<SolverIDA*> (data);
  shared_ptr<Model> model = solv->getModel();
#ifdef _DEBUG_
  if (affDebug) {
    Trace::debug() << "-> evalG(" << tres << ")" << Trace::endline;
    for (int i = 0; i < model->sizeG(); ++i) {
      Trace::debug() << i << ") gout[" << i << "]=" << gout[i] << Trace::endline;
    }
  }
#endif
  realtype *iyy = NV_DATA_S(yy);
  realtype *iyp = NV_DATA_S(yp);
  int yL = NV_LENGTH_S(yy);
  int ypL = NV_LENGTH_S(yp);
  // transmitted to a vector
  vector<double> Y(iyy, iyy + yL);
  vector<double> YP(iyp, iyp + ypL);
  // the current z is needed to evaluate g
  // however, the method is static -> we use mod
  vector<double> Z = solv->getCurrentZ();
  vector<state_g> G(model->sizeG());

  model->evalG(tres, Y, YP, Z, G);

  vector<double> gIDA(G.begin(), G.end());
  // copy to be accessible by sundials
  memcpy(gout, &gIDA[0], gIDA.size() * sizeof (gIDA[0]));

  return (0);
}

int
SolverIDA::evalJ(realtype tt, realtype cj,
        N_Vector yy, N_Vector yp, N_Vector /*rr*/,
        SUNMatrix JJ, void* data,
        N_Vector /*tmp1*/, N_Vector /*tmp2*/, N_Vector /*tmp3*/) {
#ifdef _DEBUG_
  Timer timer("SolverIDA::evalJ");
  if (affDebug) {
    Trace::debug() << "-> evalJ(" << tt << ")" << Trace::endline;
  }
#endif
  SolverIDA* solv = reinterpret_cast<SolverIDA*> (data);
  shared_ptr<Model> model = solv->getModel();

  realtype* iyy = NV_DATA_S(yy);
  realtype* iyp = NV_DATA_S(yp);

  SparseMatrix smj;
  int size = model->sizeY();
  smj.init(size, size);
  model->evalJt(tt, iyy, iyp, cj, smj);

  bool matrixStructChange = SolverCommon::copySparseToKINSOL(smj, JJ, size, solv->lastRowVals_);

  if (matrixStructChange) {
    SUNLinSol_KLUReInit(solv->LS_, JJ, SM_NNZ_S(JJ), 2);  // reinit symbolic factorisation
    if (solv->lastRowVals_ != NULL) {
      free(solv->lastRowVals_);
    }
    solv->lastRowVals_ = reinterpret_cast<sunindextype*> (malloc(sizeof (sunindextype)*SM_NNZ_S(JJ)));
    memcpy(solv->lastRowVals_, SM_INDEXVALS_S(JJ), sizeof (sunindextype)*SM_NNZ_S(JJ));
  }

  return (0);
}

void
SolverIDA::solve(double tAim, double &tNxt) {
  int flag = IDASolve(IDAMem_, tAim, &tNxt, yy_, yp_, IDA_ONE_STEP);

  string msg;
  switch (flag) {
    case IDA_SUCCESS:
      msg = "IDA_SUCCESS";
      break;
    case IDA_ROOT_RETURN:
      msg = "IDA_ROOT_RETURN";
      break;
    case IDA_TSTOP_RETURN:
      msg = "IDA_TSTOP_RETURN";
      break;
    default:
      analyseFlag(flag);
  }

  // A root has been found at tNxt
  if (flag == IDA_ROOT_RETURN) {
#ifdef _DEBUG_
    vector<state_g> rootsFound = getRootsFound();
    for (unsigned int i = 0; i < rootsFound.size(); i++) {
      if (rootsFound[i] != NO_ROOT) {
        Trace::debug() << "SolverIDA: rootsfound ->  g[" << i << "]=" << rootsFound[i] << Trace::endline;
        std::string subModelName("");
        int localGIndex(0);
        std::string gEquation("");
        model_->getGInfos(i, subModelName, localGIndex, gEquation);
        Trace::debug() << DYNLog(RootGeq, i, subModelName, gEquation) << Trace::endline;
      }
    }
#endif

    // Propagating the root change
    model_->evalG(tNxt, vYy_, vYp_, vYz_, g0_);
    ++stats_.nge_;
    evalZMode(g0_, g1_, tNxt);
  }

#ifdef _DEBUG_
  /* The convergence criterion in IDA is associated to the weighted RMS norm of the delta between two Newton iterations.
   * Indeed, the correction step is successful if sqrt(Sum(w*(y(k+1)-y(k))^2)/n) < tolerance where k is the kth Newton iteration and n the number of variables
   * The weights (w) used are inversely proportional to the relative accuracy multiplied by the value variable and the absolute accuracy.
   * The local errors are the sum of the differences between y before and after the Newton iteration (errors += y(k+1) - y(k)).
   * => Therefore the errors multiplied by the weights are a good indicator of the variables that evolve the more during the Newton iterations.
   */
  int nbY = model_->sizeY();
  int nbErr = 10;
  double thresholdErr = 1;

  if (nbErr > nbY)
      nbErr = nbY;

  // Defining necessary data structure and retrieving information from IDA
  N_Vector nvWeights = N_VNew_Serial(nbY);
  N_Vector nvErrors = N_VNew_Serial(nbY);
  if (IDAGetErrWeights(IDAMem_, nvWeights) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetErrWeights");

  if (IDAGetEstLocalErrors(IDAMem_, nvErrors) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetEstLocalErrors");

  double* weights = NV_DATA_S(nvWeights);
  double* errors = NV_DATA_S(nvErrors);

  for (int i = 0; i < nbY; ++i)
    errors[i] = fabs(weights[i] * errors[i]);

  // Filling and sorting the vector
  vector<std::pair<double, int> > yErr;
  for (int i = 0; i < nbY; ++i) {
    // Tolerances (RTOL and ATOL) are 1e-04 by default so weights are around 1e4 therefore 1 is a relatively small value
    if (errors[i] > thresholdErr) {
      yErr.push_back(std::pair<double, int>(errors[i], i));
    }
  }
  std::sort(yErr.begin(), yErr.end(), SolverCommon::mapcompabs());

  Trace::debug() << DYNLog(SolverIDALargestErrors, nbErr) << Trace::endline;
  vector<std::pair<double, int> >::iterator it;
  int i = 0;
  for (it = yErr.begin(); it != yErr.end(); ++it) {
      Trace::debug() << DYNLog(SolverIDAErrorValue, thresholdErr, it->second, it->first) << Trace::endline;
      if (i >= nbErr)
        break;
      ++i;
    }

  // Destroying the specific data structures
  N_VDestroy_Serial(nvWeights);
  N_VDestroy_Serial(nvErrors);
#endif
}

/*
 * This routine deals with the possible actions due to a mode change.
 * In IDA, in case of a mode change, depending on the types of mode, either there is an algebraic equation restoration or just
 * a simple reinitialization of the solver.
 */
void
SolverIDA::reinit(std::vector<double> &yNxt, std::vector<double> &ypNxt) {
  int counter = 0;
  modeChangeType_t modeChangeType = model_->getModeChangeType();

  if (modeChangeType != DIFFERENTIAL_MODE) {
    do {
      model_->rotateBuffers();
      state_.reset();
      model_->reinitMode();

      // During the algebraic equation restoration, the system could have moved a lot from its previous state.
      // J updates and preconditioner calls must be done on a regular basis.
      bool noInitSetup = false;
      if (modeChangeType == ALGEBRAIC_MODE) {
        if (previousReinit_ == None) {
          solverKINNormal_->init(model_, SolverKIN::KIN_NORMAL, 1e-4, 1e-4, 30, 1, 10, 100000, 0);
          solverKINYPrim_->init(model_, SolverKIN::KIN_YPRIM, 1e-4, 1e-4, 30, 1, 10, 100000, 0);
          previousReinit_ = Algebraic;
        } else if (previousReinit_ == AlgebraicWithJUpdate) {
          solverKINNormal_->modifySettings(1e-4, 1e-4, 30, 10, 100000, 0);
          solverKINYPrim_->modifySettings(1e-4, 1e-4, 30, 10, 100000, 0);
          previousReinit_ = Algebraic;
        }
        noInitSetup = true;
      } else {
        if (previousReinit_ == None) {
          solverKINNormal_->init(model_, SolverKIN::KIN_NORMAL, 1e-4, 1e-4, 30, 1, 1, 100000, 0);
          solverKINYPrim_->init(model_, SolverKIN::KIN_YPRIM, 1e-4, 1e-4, 30, 1, 1, 100000, 0);
          previousReinit_ = AlgebraicWithJUpdate;
        } else if (previousReinit_ == Algebraic) {
          solverKINNormal_->modifySettings(1e-4, 1e-4, 30, 1, 100000, 0);
          solverKINYPrim_->modifySettings(1e-4, 1e-4, 30, 1, 100000, 0);
          previousReinit_ = AlgebraicWithJUpdate;
        }
      }

      // Call to solver KIN to find new algebraic variables' values
      for (unsigned int i = 0; i < vYId_.size(); i++)
        if (vYId_[i] != DYN::DIFFERENTIAL)
          vYp_[i] = 0;

      solverKINNormal_->setInitialValues(tSolve_, vYy_, vYp_);
      solverKINNormal_->solve(noInitSetup);
      solverKINNormal_->getValues(vYy_, vYp_);

      // Recomputation of differential variables' values
      for (unsigned int i = 0; i < vYId_.size(); i++)
        if (vYId_[i] != DYN::DIFFERENTIAL)
          vYp_[i] = 0;

      solverKINYPrim_->setInitialValues(tSolve_, vYy_, vYp_);
      solverKINYPrim_->solve(noInitSetup);
      solverKINYPrim_->getValues(vYy_, vYp_);

      // Root stabilization
      model_->evalG(tSolve_, vYy_, vYp_, vYz_, g1_);
      ++stats_.nge_;
      bool rootFound = !(std::equal(g0_.begin(), g0_.end(), g1_.begin()));
      if (rootFound) {
        g0_.assign(g1_.begin(), g1_.end());
        evalZMode(g0_, g1_, tSolve_);
        modeChangeType = model_->getModeChangeType();
      } else {
        modeChangeType = NO_MODE;
      }

      counter++;
      if (counter >= 10)
        throw DYNError(Error::SOLVER_ALGO, SolverIDAUnstableRoots);
    } while (modeChangeType == ALGEBRAIC_MODE || modeChangeType == ALGEBRAIC_J_UPDATE_MODE);

    updateStatistics();
  }


  int flag0 = IDAReInit(IDAMem_, tSolve_, yy_, yp_);  // required to relaunch the simulation
  if (flag0 < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAReinit");

  // saving the new step
  yNxt = vYy_;
  ypNxt = vYp_;
}

vector<state_g>
SolverIDA::getRootsFound() const {
  Timer timer("SolverIDA::getRootsFound()");
  vector<state_g> rootsFound(model_->sizeG(), NO_ROOT);

  if (IDAGetRootInfo(IDAMem_, &rootsFound[0]) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetRootInfo");

  return rootsFound;
}

void
SolverIDA::getLastConf(long int &nst, int & kused, double & hused) {
  // number of used intern iterations
  if (IDAGetNumSteps(IDAMem_, &nst) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetNumSteps");

  if (IDAGetLastOrder(IDAMem_, &kused) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetLastOrder");

  // value of last used intern time step
  if (IDAGetLastStep(IDAMem_, &hused) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetLastStep");

  return;
}

void
SolverIDA::updateStatistics() {
  // statistics gathering
  long int nst;
  if (IDAGetNumSteps(IDAMem_, &nst) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetNumSteps");

  long int nre;
  if (IDAGetNumResEvals(IDAMem_, &nre) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetNumResEvals");

  long int nje;
  if (IDAGetNumJacEvals(IDAMem_, &nje) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDASlsGetNumJacEvals");

  long int nni;
  if (IDAGetNumNonlinSolvIters(IDAMem_, &nni) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetNumNonlinSolvIters");

  long int netf;
  if (IDAGetNumErrTestFails(IDAMem_, &netf) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetNumErrTestFails");

  long int ncfn;
  if (IDAGetNumNonlinSolvConvFails(IDAMem_, &ncfn) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetNumNonlinSolvConvFails");

  long int nge;
  if (IDAGetNumGEvals(IDAMem_, &nge) < 0)
    throw DYNError(Error::SUNDIALS_ERROR, SolverFuncErrorIDA, "IDAGetNumGEvals");

  // update solver's counters
  stats_.nst_ += nst;
  stats_.nre_ += nre;
  stats_.nje_ += nje;
  stats_.nni_ += nni;
  stats_.netf_ += netf;
  stats_.ncfn_ += ncfn;
  stats_.nge_ += nge;
}

void
SolverIDA::errHandlerFn(int error_code, const char* module, const char* function,
        char* msg, void* /*eh_data*/) {
  if (error_code == IDA_WARNING) {
    Trace::warn() << module << " " << function << " :" << msg << Trace::endline;
  } else {
    Trace::error() << module << " " << function << " :" << msg << Trace::endline;
  }
}

void
SolverIDA::printEnd() {
  updateStatistics();

  // (1) Writing on standard output
  // -----------------------------------
  Trace::debug() << Trace::endline;
  Trace::debug() << DYNLog(SolverExecutionStats) << Trace::endline;
  Trace::debug() << Trace::endline;

  Trace::debug() << DYNLog(SolverNbIter, stats_.nst_) << Trace::endline;
  Trace::debug() << DYNLog(SolverNbResEval, stats_.nre_) << Trace::endline;
  Trace::debug() << DYNLog(SolverNbJacEval, stats_.nje_) << Trace::endline;
  Trace::debug() << DYNLog(SolverNbNonLinIter, stats_.nni_) << Trace::endline;
  Trace::debug() << DYNLog(SolverNbErrorTestFail, stats_.netf_) << Trace::endline;
  Trace::debug() << DYNLog(SolverNbNonLinConvFail, stats_.ncfn_) << Trace::endline;
  Trace::debug() << DYNLog(SolverNbRootFuncEval, stats_.nge_) << Trace::endline;
}

}  // end namespace DYN
