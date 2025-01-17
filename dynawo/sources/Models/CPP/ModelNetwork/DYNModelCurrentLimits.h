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
 * @file  DYNModelCurrentLimits.h
 *
 * @brief
 *
 */
#ifndef MODELS_CPP_MODELNETWORK_DYNMODELCURRENTLIMITS_H_
#define MODELS_CPP_MODELNETWORK_DYNMODELCURRENTLIMITS_H_

#include <vector>
#include "DYNEnumUtils.h"

namespace DYN {
class ModelNetwork;

/**
 * @brief  Generic Current Limits model
 */
class ModelCurrentLimits {  ///< Generic Current Limits model
 public:
  /**
   * @brief structure for side
   */
  typedef enum {
    SIDE_UNDEFINED = 0,
    SIDE_1 = 1,
    SIDE_2 = 2,
    SIDE_3 = 3
  } side_t;

  /**
   * @brief structure for state
   */
  typedef enum {
    COMPONENT_OPEN = 0,
    COMPONENT_CLOSE = 1
  } state_t;

  /**
   * @brief default constructor
   */
  ModelCurrentLimits();

  /**
   * @brief destructor
   */
  ~ModelCurrentLimits();

  /**
   * @brief compute the local root function
   *
   * @param componentName name of the component for which the current limits is
   * @param t current time
   * @param current current inside the component
   * @param g value of the root function
   * @param desactivate @b true if the current limits is off
   */
  void evalG(const std::string& componentName, const double& t, const double& current, state_g* g, const double& desactivate);

  /**
   * @brief compute the state of the current limits
   *
   * @param componentName name of the component for which the current limits is
   * @param t current time
   * @param g buffer of the roots
   * @param network model of network
   * @param desactivate @b true if the current limits is off
   *
   * @return
   */
  state_t evalZ(const std::string& componentName, const double& t, state_g * g, ModelNetwork* network,
                const double& desactivate);  // compute the local Z function

  /**
   * @brief add a new current limit
   * @param limit
   */
  void addLimit(const double& limit);  // add a new current limit (p.u. base UNom, base SNRef)
  /**
   * @brief add a maximum duration above the limit
   * @param acceptableDuration
   */
  void addAcceptableDuration(const int& acceptableDuration);  // add a maximum duration above the limit
  /**
   * @brief set side
   * @param side
   */
  void setSide(const side_t side);
  /**
   * @brief set the overall number of current limits
   * @param number
   */
  void setNbLimits(const int& number);  // set the overall number of current limits
  /**
   * @brief set the max time operation
   * @param maxTimeOperation
   */
  void setMaxTimeOperation(const double& maxTimeOperation);

  /**
   * @brief get G size
   * @return size of G
   */
  int sizeG() const;  // get the size of the local G function
  /**
   * @brief get size of Z
   * @return size of Z
   */
  int sizeZ() const;  // get the size of the local Z function

 private:
  int nbLimits_;  ///< number of current limits
  side_t side_;  ///< side

  double maxTimeOperation_;  ///< maximum time operation, if limits duration is over this time, the current limit does not operate

  std::vector<double> limits_;  ///< vector of current limits (p.u. base UNom, base SNRef)
  std::vector<double> acceptableDurations_;  ///< vector of limits duration (unit : s)
  std::vector<bool> limitActivated_;  ///< vector describing whether each limit is activated
  std::vector<bool> openingAuthorized_;  ///< whether opening is authorized
  std::vector<double> tLimitReached_;  ///< last time the limit was reached
  std::vector<bool> activated_;  ///< state of activation
};
}  // namespace DYN

#endif  // MODELS_CPP_MODELNETWORK_DYNMODELCURRENTLIMITS_H_
