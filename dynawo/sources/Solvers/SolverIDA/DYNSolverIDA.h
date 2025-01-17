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
 * @file  DYNSolverIDA.h
 *
 * @brief Solver based on sundials/IDA solver : variable step, order one or two
 *
 */
#ifndef SOLVERS_SOLVERIDA_DYNSOLVERIDA_H_
#define SOLVERS_SOLVERIDA_DYNSOLVERIDA_H_

#include <boost/shared_ptr.hpp>
#include <vector>
#include <map>
#include <sundials/sundials_nvector.h>

#include "DYNSolverFactory.h"
#include "DYNSolverImpl.h"

namespace parameters {
class ParametersSet;
}

namespace DYN {
class SolverKIN;
/**
 * @brief SolverIDA factory class
 *
 * Class for IDA solver creation
 */
class SolverIDAFactory : public SolverFactory {
 public:
  /**
   * @brief default constructor
   */
  SolverIDAFactory();

  /**
   * @brief destructor
   */
  ~SolverIDAFactory();

  /**
   * @brief Create an instance of solver
   * @return the new instance of solver created by the factory
   */
  Solver* create() const;

  /**
   * @brief SolverIDA destroy
   */
  void destroy(Solver*) const;
};

/**
 * @brief class Solver IDA
 */
class SolverIDA : public Solver::Impl {
 public:
  /**
   * @brief default constructor
   */
  SolverIDA();

  /**
   * @brief destructor
   */
  ~SolverIDA();

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
  void init(const boost::shared_ptr<Model> & model, const double & t0, const double & tEnd);

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

  /**
   * @brief getter for the state of the solver
   * @return @b true is the solver is in init state, @b false otherwise
   */
  inline bool flagInit() const {
    return flagInit_;
  }

 private:
  /**
   * @brief update statistics of execution of the solver
   */
  void updateStatistics();

  /**
   * @brief indicates which root was activated
   * @return an array showing which root was activated
   */
  std::vector<state_g> getRootsFound() const;

  /**
   * @brief computes the problem residual for given values of time, state vector
   * y and derivative yp
   *
   * @param tres current value of the time
   * @param yy current value of the state variables y
   * @param yp current value of the derivative variables yp
   * @param rr output residual vector F(t,y,yp)
   * @param data pointer to user data (instance of solver)
   *
   * @return 0 is successful, positive value otherwise
   */
  static int evalF(double tres, N_Vector yy, N_Vector yp, N_Vector rr,
          void *data);

  /**
   * @brief computes the function g(t,y,yp) such that a root should be found during the integration
   *
   * @param tres current value of the time
   * @param yy current value of the state variables y
   * @param yp current value of the derivative variables yp
   * @param gout value of g(t,y,yp)
   * @param data pointer to user data (instance of solver)
   *
   * @return  0 is successful, positive value otherwise
   */
  static int evalG(double tres, N_Vector yy, N_Vector yp, double *gout,
          void *data);

  /**
   * @brief compute the Jacobian J of the DAE system \f$(J=@F/@y + cj @F/@yp)\f$
   *
   * @param tt current value of the time
   * @param cj scalar in the system Jacobian
   * @param yy current value of the state variables y
   * @param yp current value of the derivative variables yp
   * @param rr current value of the residual function F(t,y,yp)
   * @param JJ output jacobian
   * @param data pointer to user data (instance of solver)
   * @param tmp1 unused
   * @param tmp2 unused
   * @param tmp3 unused
   *
   * @return 0 is successful, positive value otherwise
   */
  static int evalJ(double tt, double cj,
          N_Vector yy, N_Vector yp, N_Vector rr,
          SUNMatrix JJ, void *data,
          N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);

  /**
   * @brief processes error and warning messages from IDA solver
   *
   * @param error_code error code returns by IDA
   * @param module name of the IDA module reporting error
   * @param function name of the function in which error occurred
   * @param msg error message
   * @param eh_data unused
   */
  static void errHandlerFn(int error_code, const char *module, const char *function,
          char *msg, void *eh_data);

  /**
   * @brief destroy all allocated memory
   */
  void clean();

  /**
   * @brief Analyse a flag return by the IDA solver
   *
   * @param flag flag to analyse
   */
  void analyseFlag(const int & flag);

 private:
  void* IDAMem_;  ///< IDA internal memory structure
  SUNLinearSolver LS_;  ///< Linear Solver pointer
  SUNMatrix M_;  ///< sparse SUNMatrix
  boost::shared_ptr<SolverKIN> solverKINNormal_;  ///< Newton Raphson solver for the algebraic variables restoration
  boost::shared_ptr<SolverKIN> solverKINYPrim_;  ///< Newton-Raphson solver for the derivatives of the differential variables restoration

  // parameters
  int order_;  ///< maximum order to use in the integration method
  double initStep_;  ///< initial step size
  double minStep_;  ///< minimal step size
  double maxStep_;  ///< maximum step size
  double absAccuracy_;  ///< relative error tolerance
  double relAccuracy_;  ///< absolute error tolerance

  bool flagInit_;  ///< @b true if the solver is in initialization mode

  sunindextype* lastRowVals_;  ///< save of last Jacobian structure, to force symbolic factorization if structure change
};

}  // end of namespace DYN

#endif  // SOLVERS_SOLVERIDA_DYNSOLVERIDA_H_
