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
 * @file JOBSimulationEntryImpl.h
 * @brief Simulation entries description : header file
 *
 */

#ifndef API_JOB_JOBSIMULATIONENTRYIMPL_H_
#define API_JOB_JOBSIMULATIONENTRYIMPL_H_

#include <string>
#include "JOBSimulationEntry.h"

namespace job {

/**
 * @class SimulationEntry::Impl
 * @brief SimulationEntry implemented class
 */
class SimulationEntry::Impl : public SimulationEntry {
 public:
  /**
   * @brief Default constructor
   */
  Impl();

  /**
   * @brief destructor
   */
  virtual ~Impl();

  /**
   * @copydoc SimulationEntry::setStartTime()
   */
  void setStartTime(const double & startTime);

  /**
   * @copydoc SimulationEntry::getStartTime()
   */
  double getStartTime() const;

  /**
   * @copydoc SimulationEntry::setStopTime()
   */
  void setStopTime(const double & stopTime);

  /**
   * @copydoc SimulationEntry::getStopTime()
   */
  double getStopTime() const;

  /**
   * @copydoc SimulationEntry::setActivateCriteria()
   */
  void setActivateCriteria(bool activate);

  /**
   * @copydoc SimulationEntry::getActivateCriteria()
   */
  bool getActivateCriteria() const;

  /**
   * @copydoc SimulationEntry::setCriteriaStep()
   */
  void setCriteriaStep(const int & criteriaStep);

  /**
   * @copydoc SimulationEntry::getCriteriaStep()
   */
  int getCriteriaStep() const;

 private:
  double startTime_;  ///< Start time of the simulation
  double stopTime_;  ///< Stop time of the simulation
  bool activateCriteria_;  ///< Whether to activate the verification of criteria
  int criteriaStep_;  ///< criteria verification time step
};

}  // namespace job

#endif  // API_JOB_JOBSIMULATIONENTRYIMPL_H_
