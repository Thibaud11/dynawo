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
 * @file  DYNCommon.cpp
 *
 * @brief  define common function that could be used
 *
 */
#include <string>
#include <stdio.h>
#include <cstring>
#include <cassert>
#include <cstdarg>
#include <sstream>
#include <iomanip>
#include "DYNCommon.h"
#include "DYNMacrosMessage.h"

using std::string;

namespace DYN {

const char* sharedLibraryExtension() {
#ifdef _WIN32
static const char extension[] = ".dll";
#elif __APPLE__
static const char extension[] = ".dylib";
#elif __linux__
static const char extension[] = ".so";
#elif __unix__  // all unices not caught above
static const char extension[] = ".so";
#else
#error "Unknown compiler"
#endif
  return extension;
}

bool vectorAreEquals(const std::vector<double> & y1, const std::vector<double> & y2) {
  if (y1.size() != y2.size())
    return false;

  if (std::equal(y1.begin(), y1.end(), y2.begin()))
    return true;
  return false;
}

std::string double2String(const double& value, const int& nbDecimal) {
  std::stringstream ss("");
  ss << std::setprecision(nbDecimal) << std::fixed << value;
  return ss.str();
}

string typeVarC2Str(const typeVarC_t& type) {
  string typeVarC;
  switch (type) {
    case VAR_TYPE_STRING:
      typeVarC = "STRING";
      break;
    case VAR_TYPE_DOUBLE:
      typeVarC = "DOUBLE";
      break;
    case VAR_TYPE_INT:
      typeVarC = "INT";
      break;
    case VAR_TYPE_BOOL:
      typeVarC = "BOOL";
      break;
  }
  return typeVarC;
}

typeVarC_t str2TypeVarC(const std::string& typeStr) {
  if (typeStr == "STRING")
    return VAR_TYPE_STRING;
  else if (typeStr == "DOUBLE")
    return VAR_TYPE_DOUBLE;
  else if (typeStr == "INT")
    return VAR_TYPE_INT;
  else if (typeStr == "BOOL")
    return VAR_TYPE_BOOL;
  else
    throw DYNError(Error::MODELER, TypeVarCUnableToConvert, typeStr);
}

int sign(const double& value) {
  return (value < 0.) ? -1 : 1;
}

}  // namespace DYN
