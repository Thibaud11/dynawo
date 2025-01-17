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

/*
 * @file Modeler/Common/test/Test.cpp
 * @brief Unit tests for common Modeler methods
 *
 */
#include <boost/filesystem.hpp>
#include <string>

#include "DYNCommon.h"
#include "DYNExecUtils.h"
#include "DYNFileSystemUtils.h"

#include "gtest_dynawo.h"


using boost::shared_ptr;

namespace DYN {

TEST(ModelicaCompilerTestSuite, BasicCompilationTest) {
  std::string varExtCommand = "python Scripts_OMC_1_13_2/writeModel.py -m GeneratorPQ  -i ModelicaModel/ -o res/ --init";

  remove_all_in_directory("res");
  boost::filesystem::path fspath("res");
  boost::filesystem::remove(fspath);
  create_directory("res");
  std::stringstream ssPython;
  executeCommand(varExtCommand, ssPython);
  ASSERT_EQ(ssPython.str(), "Executing command : " + varExtCommand + "\n");
  std::cout << ssPython.str() << std::endl;
  std::stringstream ssDiff;
  executeCommand("diff ModelicaModel/reference res/", ssDiff);
  std::cout << ssDiff.str() << std::endl;
  ASSERT_EQ(ssDiff.str(), "Executing command : diff ModelicaModel/reference res/\n");
  remove_all_in_directory("res");
  ASSERT_EQ(boost::filesystem::remove(fspath), true);
}

TEST(ModelicaCompilerTestSuite, TestPackageOption) {
  std::string varExtCommand = "../compileModelicaModel --model Test --lib Test" + std::string(sharedLibraryExtension()) +
          " --model-dir . --compilation-dir compilation --package-name Test";

  remove_all_in_directory("compilation");
  boost::filesystem::path fspath("compilation");
  boost::filesystem::remove(fspath);
  std::stringstream ssCompileModelicaModel;
  executeCommand(varExtCommand, ssCompileModelicaModel);
  std::cout << ssCompileModelicaModel.str() << std::endl;
  ASSERT_EQ(boost::filesystem::exists("Test" + std::string(sharedLibraryExtension())), true);
}

}  // namespace DYN
