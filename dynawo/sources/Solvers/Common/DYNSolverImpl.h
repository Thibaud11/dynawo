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
 * @file  DYNSolverImpl.h
 *
 * @brief Dynawo solvers : header file
 *
 */
#ifndef SOLVERS_COMMON_DYNSOLVERIMPL_H_
#define SOLVERS_COMMON_DYNSOLVERIMPL_H_

#include <vector>
#include <string>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/core/noncopyable.hpp>
#include <sundials/sundials_nvector.h>

#include "DYNSolver.h"
#include "DYNEnumUtils.h"
#include "DYNParameterSolver.h"

namespace parameters {
class ParametersSet;
}

namespace timeline {
class Timeline;
}

namespace DYN {


/**
 * @brief Structure for storing solver statistics
 */
typedef struct {
  long int nst_;  ///< number of steps
  long int nre_;  ///< number of residual evaluations
  long int nje_;  ///< number of Jacobian evaluations
  long int nni_;  ///< number of nonlinear iterations
  long int netf_;  ///< number of error test failures
  long int ncfn_;  ///< number of nonlinear convergence failures
  long int nge_;  ///< number of root function evaluations
} stat_t;

class Message;
class MessageTimeline;
class Model;
class ParameterSolver;

/**
 * @class Solver::Impl
 * @brief Solver implemented class
 *
 * Implementation of Solver interface class
 */
class Solver::Impl : public Solver, private boost::noncopyable {
 public:
  /**
   * @brief Constructor
   */
  Impl();

  /**
   * @brief Destructor
   */
  virtual ~Impl();

  /**
   * @copydoc Solver::getState()
   */
  inline const BitMask& getState() const {
    return state_;
  }

  /**
   * @copydoc Solver::setPreviousReinit(const PreviousReinit& previousReinit)
   */
  inline void setPreviousReinit(const PreviousReinit& previousReinit) {
    previousReinit_ = previousReinit;
  }

  /**
   * @copydoc Solver::getPreviousReinit()
   */
  inline const PreviousReinit& getPreviousReinit() const {
    return previousReinit_;
  }
  /**
   * @copydoc Solver::setParameters(const boost::shared_ptr<parameters::ParametersSet> &params)
   */
  void setParameters(const boost::shared_ptr<parameters::ParametersSet> &params);

  /**
   * @copydoc Solver::defineParameters()
   */
  virtual void defineParameters() = 0;

  /**
   * @copydoc Solver::solverType()
   */
  virtual std::string solverType() const = 0;

  /**
   * @copydoc Solver::hasParameter(const std::string &nameParameter)
   */
  bool hasParameter(const std::string &nameParameter);

  /**
   * @copydoc Solver::getParametersMap()
   */
  const std::map<std::string, ParameterSolver>& getParametersMap() const;

  /**
   * @copydoc Solver::checkUnusedParameters()
   */
  void checkUnusedParameters(boost::shared_ptr<parameters::ParametersSet> params);

  /**
   * @copydoc Solver::findParameter(const std::string &name)
   */
  ParameterSolver& findParameter(const std::string &name);

  /**
   * @copydoc Solver::setParameterFromSet(const std::string &parName, const boost::shared_ptr<parameters::ParametersSet> parametersSet)
   */
  void setParameterFromSet(const std::string &parName, const boost::shared_ptr<parameters::ParametersSet> parametersSet);

  /**
   * @copydoc Solver::setParametersFromPARFile(const boost::shared_ptr<parameters::ParametersSet> &params)
   */
  void setParametersFromPARFile(const boost::shared_ptr<parameters::ParametersSet> &params);

  /**
   * @brief set the solver parameters value
   */
  virtual void setSolverParameters() = 0;

  /**
   * @copydoc Solver::init(const boost::shared_ptr<Model> &model, const double &t0, const double &tEnd)
   */
  virtual void init(const boost::shared_ptr<Model> &model, const double &t0, const double &tEnd) = 0;

  /**
   * @copydoc Solver::calculateIC()
   */
  virtual void calculateIC() = 0;

  /**
   * @copydoc Solver::solve(double tAim, double &tNxt, std::vector<double> &yNxt, std::vector<double> &ypNxt)
   */
  void solve(double tAim, double &tNxt, std::vector<double> &yNxt, std::vector<double> &ypNxt);

  /**
   * @copydoc Solver::solve(double tAim, double &tNxt)
   */
  virtual void solve(double tAim, double &tNxt) = 0;

  /**
   * @copydoc Solver::reinit(std::vector<double> &yNxt, std::vector<double> &ypNxt)
   */
  virtual void reinit(std::vector<double> &yNxt, std::vector<double> &ypNxt) = 0;

  /**
   * @copydoc Solver::printSolve()
   */
  void printSolve();

  /**
   * @copydoc Solver::printHeader()
   */
  void printHeader();

  /**
   * @copydoc Solver::printEnd()
   */
  virtual void printEnd() = 0;

  /**
   * @copydoc Solver::getCurrentZ()
   */
  inline std::vector<double>& getCurrentZ() {
    return vYz_;
  }

  /**
   * @copydoc Solver::getCurrentY()
   */
  inline std::vector<double>& getCurrentY() {
    return vYy_;
  }

  /**
   * @copydoc Solver::getCurrentYP()
   */
  inline std::vector<double>& getCurrentYP() {
    return vYp_;
  }

  /**
   * @copydoc Solver::getTSolve()
   */
  inline double getTSolve() const {
    return tSolve_;
  }

  /**
   * @copydoc Solver::setTimeline(const boost::shared_ptr<timeline::Timeline> &timeline)
   */
  void setTimeline(const boost::shared_ptr<timeline::Timeline> &timeline);

  /**
   * @copydoc Solver::getLastConf(long int &nst, int &kused, double &hused)
   */

  virtual void getLastConf(long int &nst, int &kused, double &hused) = 0;

 protected:
  /**
   * @brief set a given parameter value
   *
   * @param parameter: the desired parameter
   * @param value : the value to set
   */
  template <typename T> void setParameterValue(ParameterSolver &parameter, const T &value);

  /**
   * @brief initialize all internal structure of the solver
   * @param t0 : initial time of the simulation
   * @param model : model to simulate, gives the size of all internal structure
   */
  void init(const double& t0, const boost::shared_ptr<Model> &model);

  /**
   * @brief delete all internal structure allocated by init method
   */
  void clean();

  /**
   * @brief getter for the model currently simulated
   * @return the model currently simulated
   */
  inline boost::shared_ptr<Model> getModel() const {
    return model_;
  }

  /**
   * @brief reset the execution statistics of the solver
   */
  void resetStats();

  /**
   * @brief evaluate discretes variables values and values of zero crossing functions
   * @param G0 previous value of zero crossing functions
   * @param G1 new value of zero crossing functions
   * @param time time to use to evaluate these values
   *
   * @return @b true zero crossing functions or discretes variables have changed
   */
  bool evalZMode(std::vector<state_g> &G0, std::vector<state_g> &G1, const double &time);

  /**
   * @brief detect if one zero crossing functions have changed its value
   *
   * @param vGout0 previous value of zero crossing functions
   * @param vGout1 new value of zero crossing functions
   * @param time time to use to evaluate these values
   *
   * @return @b true zero crossing functions have changed
   */
  bool detectUnstableRoot(std::vector<state_g> &vGout0, std::vector<state_g> &vGout1, const double &time);

 protected:
  std::map<std::string, ParameterSolver> parameters_;  ///< map between parameters and parameters' names
  boost::shared_ptr<Model> model_;  ///< model currently simulated
  boost::shared_ptr<timeline::Timeline> timeline_;  ///< timeline where event messages should be added or removed

  std::vector<state_g> g0_;  ///< previous values of root functions
  std::vector<state_g> g1_;  ///< updated values of root functions

  N_Vector yy_;  ///< continuous variables values stored in sundials structure
  N_Vector yp_;  ///<  derivative of variables stored in sundials structure
  N_Vector yz_;  ///< discrete variables values stored in sundials structure
  N_Vector yId_;  ///< property of variables (algebraic/differential) stored in sundials structure
  std::vector<double> vYy_;  ///< continuous variables values
  std::vector<double> vYp_;  ///< derivative of variables
  std::vector<double> vYz_;  ///< discrete variables values
  std::vector<DYN::propertyContinuousVar_t> vYId_;  ///< property of variables (algebraic/differential)

  stat_t stats_;  ///< execution statistics of the solver
  double tSolve_;  ///< current internal time of the solver
  BitMask state_;  ///< current state value of the solver
  PreviousReinit previousReinit_;  ///< previous reinitialization status of the solver
};

}  // end of namespace DYN

#include "DYNSolverImpl.hpp"

#endif  // SOLVERS_COMMON_DYNSOLVERIMPL_H_
