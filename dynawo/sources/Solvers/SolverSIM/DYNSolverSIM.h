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
 * @file  DYNSolverSIM.h
 *
 * @brief Solver based on backward Euler integration method using KINSOL
 * to solve each step
 *
 */
#ifndef SOLVERS_SOLVERSIM_DYNSOLVERSIM_H_
#define SOLVERS_SOLVERSIM_DYNSOLVERSIM_H_

#include <vector>
#include <boost/shared_ptr.hpp>
#include <sundials/sundials_nvector.h>

#include "DYNSolverFactory.h"
#include "DYNSolverImpl.h"
#include "DYNEnumUtils.h"

namespace parameters {
class ParametersSet;
}

namespace DYN {
class SolverEulerKIN;
class SolverKIN;

/**
 * @brief SolverSIM factory class
 *
 * Class for solver SIM creation
 */
class SolverSIMFactory : public SolverFactory {  ///< Simplified solver factory
 public:
  /**
   * @brief default constructor
   */
  SolverSIMFactory();

  /**
   * @brief destructor
   */
  ~SolverSIMFactory();

  /**
   * @brief Create an instance of solver
   * @return the new instance of solver created by the factory
   */
  Solver* create() const;

  /**
   * @brief SolverSIM destroy
   */
  void destroy(Solver*) const;
};

/**
 * @brief class Solver SIM : simplified solver
 */
class SolverSIM : public Solver::Impl {
 public:
  /**
   * @brief define returns value by the solver
   */
  typedef enum {
    CONV = 0,  ///< the Newton-Raphson solver finds a solution
    NON_CONV = 1,  ///< the Newton-Raphson solver doesn't find a solution
    ROOT_ALG = 2,  ///< an algebraic mode has been detected
    ROOT = 3  ///< a root was found (discrete value change)
  } SolverStatus_t;

 public:
  /**
   * @brief default constructor
   */
  SolverSIM();

  /**
   * @brief destructor
   */
  ~SolverSIM();

  /**
   * @copydoc Solver::Impl::defineParameters()
   */
  void defineParameters();

  /**
   * @copydoc Solver::Impl::setSolverParameters()
   */
  void setSolverParameters();

  /**
   * @copydoc Solver::Impl::solverType()
   */
  std::string solverType() const;

  /**
   * @copydoc Solver::init(const boost::shared_ptr<Model> & model, const double & t0, const double & tEnd)
   */
  void init(const boost::shared_ptr<Model> &model, const double & t0, const double & tEnd);
  /**
   * @copydoc Solver::solve(double tAim, double &tNxt)
   */
  void solve(double tAim, double &tNxt);

  /**
   * @copydoc Solver::reinit(std::vector<double> &yNxt, std::vector<double> &ypNxt)
   */
  void reinit(std::vector<double> &yNxt, std::vector<double> &ypNxt);

  /**
   * @copydoc Solver::calculateIC()
   */
  void calculateIC();
  /**
   * @copydoc Solver::Impl::getLastConf(long int &nst, int & kused, double & hused)
   */
  void getLastConf(long int &nst, int & kused, double & hused);

  /**
   * @brief print a summary of the execution statistics of the solver
   */
  void printEnd();

 private:
  /**
   * @brief integrate the DAE over an interval and recalculate it in case of a z change
   * @param tNxt next time step
   */
  void solveWithStepRecalculation(double &tNxt);

  /**
   * @brief integrate the DAE over an interval without recalculting the step in case of a z change
   * @param tNxt next time step
   */
  void solveWithoutStepRecalculation(double &tNxt);

  /**
   * @brief save the initial values of roots, y, yp and z before the time step
   */
  void saveInitialValues();

  /**
   * @brief increment the counter of NR tries and stop the simulation if it is higher than a threshold
   * @param counter the current number of tries
   */
  void handleMaximumTries(int& counter);

  /**
   * @brief find the solution of the problem for t+h (h is the step)
   *
   * @return the current status of the solver
   */
  SolverStatus_t solve();

  /**
   * @brief find the solution of f(u(t+h))
   *
   * @return @b 0 if the solver found a solution
   */
  int SIMCorrection();

  /**
   * @brief call the euler kin solver to find the solution
   *
   * @return @b 0 if the solver found a solution
   */
  int callSolverEulerKIN();

  /**
   * @brief update the solver attributes and strategy following a divergence
   * @param redoStep indicates if the step has to be recalculated or not
   */
  void handleDivergence(bool& redoStep);

  /**
   * @brief calculate the new step to use after divergence
   */
  void updateStepDivergence();

  /**
   * @brief restore y, yp and possibly z and roots to their initial values
   *
   * @param zRestoration @b 1 if the z values also have to be restored to their initial values
   * @param rootRestoration @b 1 if the root values also have to be restored to their initial values
   */
  void restoreInitialValues(bool zRestoration, bool rootRestoration);

  /**
   * @brief update the solver attributes and strategy following a convergence
   * @param redoStep indicates if the step has to be recalculated or not
   */
  void handleConvergence(bool& redoStep);

  /**
   * @brief calculate the new step to use after convergence
   */
  void updateStepConvergence();

  /**
   * @brief update the solver attributes and strategu following an algebraic root detection
   * @param redoStep indicates if the step has to be recalculated or not
   */
  void handleAlgebraicRoot(bool& redoStep);

  /**
   * @brief update the time step at the end of the current step
   * @param tNxt current time step calculated
   */
  void updateTimeStep(double& tNxt);

 private:
  boost::shared_ptr<SolverEulerKIN> solverEulerKIN_;  ///< Backward Euler solver
  boost::shared_ptr<SolverKIN> solverKIN_;  ///< Newton Raphson solver for the algebraic variables restoration

  // Generic and alterable parameters
  double hMin_;  ///< minimum time-step
  double hMax_;  ///< maximum time-step
  double kReduceStep_;  ///< factor to reduce the time-step
  int nEff_;  ///< desired number of Newton iterations
  int nDeadband_;  ///< deadband (iterations number) to avoid too frequent step size variations
  int maxRootRestart_;  ///< maximum number of Newton resolutions leading to root changes for one time-step
  int maxNewtonTry_;  ///< maximum number of Newton resolutions for one time-step
  std::string linearSolverName_;  ///< name of the linear solver (KLU or NICSLU at the moment)
  bool recalculateStep_;  ///< step recalculation in case of root detection

  double tEnd_;  ///< simulation end time
  double h_;  ///< current time step
  double hNew_;  ///< next time-step
  long int nNewt_;  ///< number of newton iterations since the beginning of the simulation
  int countRestart_;  ///< current number of consecutive Newton resolutions leading to root changes
  bool factorizationForced_;  ///< force the Jacobian calculation due to an algebraic mode or a non convergence of the previous NR

  std::vector<double> ySave_;  ///< values of state variables before step
  std::vector<double> ypSave_;  ///< values of derivative variables before step
  std::vector<double> zSave_;  ///< values of discrete variables before step
  std::vector<state_g> gSave_;  ///< values of roots before step
};

}  // end of namespace DYN

#endif  // SOLVERS_SOLVERSIM_DYNSOLVERSIM_H_
