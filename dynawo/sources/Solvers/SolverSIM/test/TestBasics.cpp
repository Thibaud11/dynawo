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
 * @file Solvers/SolverSIM/TestBasics.cpp
 * @brief Unit tests for Solver SIM
 *
 */

#include <fstream>
#include <cmath>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <IIDM/xml/import.h>
#include <IIDM/xml/export.h>
#include <IIDM/Network.h>
#include <IIDM/components/Connection.h>
#include <IIDM/components/ConnectionPoint.h>
#include <IIDM/components/Bus.h>
#include <IIDM/components/VoltageLevel.h>
#include <IIDM/components/Substation.h>
#include <IIDM/components/Line.h>
#include <IIDM/components/Load.h>
#include <IIDM/components/Switch.h>
#include <IIDM/builders/NetworkBuilder.h>
#include <IIDM/builders/VoltageLevelBuilder.h>
#include <IIDM/builders/BusBuilder.h>
#include <IIDM/builders/SubstationBuilder.h>
#include <IIDM/builders/LineBuilder.h>
#include <IIDM/builders/LoadBuilder.h>
#include <IIDM/builders/SwitchBuilder.h>

#include "gtest_dynawo.h"
#include "DYNDataInterfaceIIDM.h"
#include "DYNFileSystemUtils.h"
#include "DYNExecUtils.h"
#include "DYNSolver.h"
#include "DYNModeler.h"
#include "DYNModel.h"
#include "DYNModelMulti.h"
#include "DYNCompiler.h"
#include "DYNSolverFactory.h"
#include "DYNDynamicData.h"
#include "DYNParameterSolver.h"
#include "PARParametersSet.h"
#include "PARParameterFactory.h"
#include "PARParametersSetFactory.h"
#include "TLTimelineFactory.h"
#include "DYNTrace.h"

namespace DYN {

boost::shared_ptr<Solver> initSolver(const double& tStart, const double& tStop, const bool& recalculateStep, const int& maxRootRestart) {
  // Solver
  boost::shared_ptr<Solver> solver;
  solver.reset(SolverFactory::createSolverFromLib("libdynawo_SolverSIM.so"));

  boost::shared_ptr<parameters::ParametersSet> params = parameters::ParametersSetFactory::newInstance("MySolverParam");
  params->addParameter(parameters::ParameterFactory::newParameter("hMin", 0.000001));
  params->addParameter(parameters::ParameterFactory::newParameter("hMax", 1.));
  params->addParameter(parameters::ParameterFactory::newParameter("kReduceStep", 0.5));
  params->addParameter(parameters::ParameterFactory::newParameter("nEff", 10));
  params->addParameter(parameters::ParameterFactory::newParameter("nDeadband", 2));
  params->addParameter(parameters::ParameterFactory::newParameter("maxRootRestart", maxRootRestart));
  params->addParameter(parameters::ParameterFactory::newParameter("maxNewtonTry", 10));
  params->addParameter(parameters::ParameterFactory::newParameter("linearSolverName", std::string("KLU")));
  params->addParameter(parameters::ParameterFactory::newParameter("recalculateStep", recalculateStep));
  solver->setParameters(params);

  return solver;
}

void compile(boost::shared_ptr<DynamicData> dyd) {
  bool preCompiledUseStandardModels = false;
  std::vector <UserDefinedDirectory> precompiledModelsDirsAbsolute;
  std::string preCompiledModelsExtension = ".so";
  bool modelicaUseStandardModels = false;

  std::vector <UserDefinedDirectory> modelicaModelsDirsAbsolute;
  UserDefinedDirectory modelicaModel;
  modelicaModel.path = getEnvVar("PWD") +"/jobs/";
  modelicaModel.isRecursive = false;
  modelicaModelsDirsAbsolute.push_back(modelicaModel);
  std::string modelicaModelsExtension = ".mo";

  std::vector<std::string> additionalHeaderFiles;
  if (hasEnvVar("DYNAWO_HEADER_FILES_FOR_PREASSEMBLED")) {
    std::string headerFileList = getEnvVar("DYNAWO_HEADER_FILES_FOR_PREASSEMBLED");
    boost::split(additionalHeaderFiles, headerFileList, boost::is_any_of(" "), boost::token_compress_on);
  }
  Compiler cf = Compiler(dyd, preCompiledUseStandardModels,
            precompiledModelsDirsAbsolute,
            preCompiledModelsExtension,
            modelicaUseStandardModels,
            modelicaModelsDirsAbsolute,
            modelicaModelsExtension,
            additionalHeaderFiles,
            getEnvVar("PWD") +"/jobs");
  cf.compile();  // modelOnly = false, compilation and parameter linking
  cf.concatConnects();
  cf.concatRefs();
}

boost::shared_ptr<Model> initModel(const double& tStart, Modeler modeler) {
  boost::shared_ptr<Model> model = modeler.getModel();
  model->initBuffers();
  model->setIsInitProcess(true);
  model->init(tStart);
  model->rotateBuffers();
  model->printMessages();
  model->setIsInitProcess(false);
  boost::shared_ptr<timeline::Timeline> timeline = timeline::TimelineFactory::newInstance("timeline");
  model->setTimeline(timeline);
  return model;
}

std::pair<boost::shared_ptr<Solver>, boost::shared_ptr<Model> > initSolverAndModel(std::string dydFileName, std::string iidmFileName,
 std::string parFileName, const double& tStart, const double& tStop, const bool& recalculateStep, const int& maxRootRestart) {
  boost::shared_ptr<Solver> solver = initSolver(tStart, tStop, recalculateStep, maxRootRestart);

  // DYD
  boost::shared_ptr<DynamicData> dyd(new DynamicData());
  IIDM::xml::xml_parser parser;
  IIDM::Network networkIIDM = parser.from_xml(iidmFileName, false);
  boost::shared_ptr<DataInterface> data(new DataInterfaceIIDM(networkIIDM));
  boost::dynamic_pointer_cast<DataInterfaceIIDM>(data)->initFromIIDM();
  dyd->setDataInterface(data);
  dyd->setRootDirectory(getEnvVar("PWD"));
  dyd->getNetworkParameters(parFileName, "0");

  std::vector <std::string> fileNames;
  fileNames.push_back(dydFileName);
  dyd->initFromDydFiles(fileNames);

  compile(dyd);

  data->mapConnections();

  std::string ddb_dir = getEnvVar("PWD") + "/../../../Models/CPP/ModelNetwork/";
  setenv("DYNAWO_DDB_DIR", ddb_dir.c_str(), 0);
  // Model
  Modeler modeler;
  modeler.setDataInterface(data);
  modeler.setDynamicData(dyd);
  modeler.initSystem();

  boost::shared_ptr<Model> model = initModel(tStart, modeler);

  solver->init(model, tStart, tStop);

  return std::make_pair(solver, model);
}

std::pair<boost::shared_ptr<Solver>,  boost::shared_ptr<Model> > initSolverAndModelWithDyd(std::string dydFileName,
 const double& tStart, const double& tStop, const bool& recalculateStep, const int& maxRootRestart) {
  boost::shared_ptr<Solver> solver = initSolver(tStart, tStop, recalculateStep, maxRootRestart);
  // DYD
  boost::shared_ptr<DynamicData> dyd(new DynamicData());
  std::vector <std::string> fileNames;
  fileNames.push_back(dydFileName);
  dyd->initFromDydFiles(fileNames);

  compile(dyd);

  // Model
  Modeler modeler;
  // modeler.setDataInterface(data);
  modeler.setDynamicData(dyd);
  modeler.initSystem();

  boost::shared_ptr<Model> model = initModel(tStart, modeler);

  solver->init(model, tStart, tStop);

  return std::make_pair(solver, model);
}

TEST(SimulationTest, testSolverSIMTestAlpha) {
  const double tStart = 0.;
  const double tStop = 5.;
  std::pair<boost::shared_ptr<Solver>,  boost::shared_ptr<Model> > p = initSolverAndModelWithDyd("jobs/solverTestAlpha.dyd", tStart, tStop, false, 3);
  boost::shared_ptr<Solver> solver = p.first;
  boost::shared_ptr<Model> model = p.second;

  solver->calculateIC();

  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeY(), 2);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeF(), 2);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeG(), 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeZ(), 0);
  std::vector<double> y0(model->sizeY());
  std::vector<double> yp0(model->sizeY());
  std::vector<double> z0(model->sizeZ());
  model->getY0(tStart, y0, yp0, z0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y0[0], -2);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y0[1], -1);
  // At the initialization step, only the algebraic equations are considered - yp() = 0.
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp0[0], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp0[1], 0);


  bool algebraicMode = false;
  double tCurrent = tStart;
  std::vector<double> y(y0);
  std::vector<double> yp(yp0);
  std::vector<double> z(z0);
  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], -1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], -1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[1], 0);

  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], -1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[1], 0);

  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[1], 0);

  ASSERT_EQ(solver->solverType(), "SimplifiedSolver");
  solver->printHeader();
  solver->printSolve();
  solver->printEnd();
}

TEST(SimulationTest, testSolverSIMTestBeta) {
  const double tStart = 0.;
  const double tStop = 5.;
  std::pair<boost::shared_ptr<Solver>,  boost::shared_ptr<Model> > p = initSolverAndModelWithDyd("jobs/solverTestBeta.dyd", tStart, tStop, false, 3);
  boost::shared_ptr<Solver> solver = p.first;
  boost::shared_ptr<Model> model = p.second;

  solver->calculateIC();

  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeY(), 2);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeF(), 2);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeG(), 4);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeZ(), 1);
  std::vector<double> y0(model->sizeY());
  std::vector<double> yp0(model->sizeY());
  std::vector<double> z0(model->sizeZ());
  model->getY0(tStart, y0, yp0, z0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y0[0], -2);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y0[1], -1);
  // At the initialization step, only the algebraic equations are considered - yp() = 0.
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp0[0], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z0[0], -1);


  bool algebraicMode = false;
  double tCurrent = tStart;
  std::vector<double> y(y0);
  std::vector<double> yp(yp0);
  std::vector<double> z(z0);
  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], -1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], -1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z[0], -1);

  // At this stage, z has been updated but y has not yet been updated with the current strategy
  // Indeed, the algebraic and differential continuous variables are calculated at the time step beginning.
  // Then there is the discrete variable update that is done but algebraic and differential continuous variables aren't refreshed.
  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], -1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z[0], 1);

  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z[0], 1);
}

TEST(SimulationTest, testSolverSIMTestBetaUnstableRoot) {
  const double tStart = 0.;
  const double tStop = 5.;
  // Here the maximum root restart is artificially set at zero to test the maximum root restart detection in the simplified solver strategy.
  std::pair<boost::shared_ptr<Solver>,  boost::shared_ptr<Model> > p = initSolverAndModelWithDyd("jobs/solverTestBeta.dyd", tStart, tStop, false, 0);
  boost::shared_ptr<Solver> solver = p.first;
  boost::shared_ptr<Model> model = p.second;

  solver->calculateIC();

  std::vector<double> y0(model->sizeY());
  std::vector<double> yp0(model->sizeY());
  std::vector<double> z0(model->sizeZ());
  model->getY0(tStart, y0, yp0, z0);

  bool algebraicMode = false;
  double tCurrent = tStart;
  std::vector<double> y(y0);
  std::vector<double> yp(yp0);
  std::vector<double> z(z0);
  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);

  // Max root restart hit at t = 2s. Accept the time step in the current strategy.
  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], -1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z[0], 1);

  ASSERT_DOUBLE_EQUALS_DYNAWO(tCurrent, 2);
  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
}

TEST(SimulationTest, testSolverSIMTestBetaWithRecalculation) {
  const double tStart = 0.;
  const double tStop = 5.;
  std::pair<boost::shared_ptr<Solver>,  boost::shared_ptr<Model> > p = initSolverAndModelWithDyd("jobs/solverTestBeta.dyd", tStart, tStop, true, 3);
  boost::shared_ptr<Solver> solver = p.first;
  boost::shared_ptr<Model> model = p.second;

  solver->calculateIC();

  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeY(), 2);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeF(), 2);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeG(), 4);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeZ(), 1);
  std::vector<double> y0(model->sizeY());
  std::vector<double> yp0(model->sizeY());
  std::vector<double> z0(model->sizeZ());
  model->getY0(tStart, y0, yp0, z0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y0[0], -2);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y0[1], -1);
  // At the initialization step, only the algebraic equations are considered - yp() = 0.
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp0[0], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z0[0], -1);


  bool algebraicMode = false;
  double tCurrent = tStart;
  std::vector<double> y(y0);
  std::vector<double> yp(yp0);
  std::vector<double> z(z0);
  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], -1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], -1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z[0], -1);

  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z[0], 1);

  // At this stage, contrary to the scheme without recalculation, algebraic and differential variables are recalculated.
  // It explains why we get here z = 1 and y = 1.
  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(yp[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z[0], 1);
}

TEST(SimulationTest, testSolverSIMDivergenceWithRecalculation) {
  const double tStart = 0.;
  const double tStop = 3.;
  std::pair<boost::shared_ptr<Solver>,  boost::shared_ptr<Model> > p = initSolverAndModelWithDyd("jobs/solverTestGamma.dyd", tStart, tStop, true, 3);
  boost::shared_ptr<Solver> solver = p.first;
  boost::shared_ptr<Model> model = p.second;

  solver->calculateIC();

  ASSERT_DOUBLE_EQUALS_DYNAWO(model->sizeY(), 2);
  std::vector<double> y0(model->sizeY());
  std::vector<double> yp0(model->sizeY());
  std::vector<double> z0(model->sizeZ());
  model->getY0(tStart, y0, yp0, z0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y0[0], 1);

  bool algebraicMode = false;
  double tCurrent = tStart;
  std::vector<double> y(y0);
  std::vector<double> yp(yp0);
  std::vector<double> z(z0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(tCurrent, 0);

  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(tCurrent, 1);

  // Divergence at t=2, reduce the time step and resolve at t=1.5
  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], 0.8);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[1], 0.8);
  ASSERT_DOUBLE_EQUALS_DYNAWO(tCurrent, 1.5);

  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[0], 0.8);
  ASSERT_DOUBLE_EQUALS_DYNAWO(tCurrent, 2.5);

  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(tCurrent, 3);
}

TEST(SimulationTest, testSolverSIMAlgebraicMode) {
  const double tStart = 0.;
  const double tStop = 3.;
  std::pair<boost::shared_ptr<Solver>,  boost::shared_ptr<Model> > p = initSolverAndModel("jobs/solverTestDelta.dyd",
  "jobs/solverTestDelta.iidm", "jobs/solverTestDelta.par", tStart, tStop, true, 3);
  boost::shared_ptr<Solver> solver = p.first;
  boost::shared_ptr<Model> model = p.second;

  solver->calculateIC();

  std::vector<double> y0(model->sizeY());
  std::vector<double> yp0(model->sizeY());
  std::vector<double> z0(model->sizeZ());
  model->getY0(tStart, y0, yp0, z0);

  bool algebraicMode = false;
  double tCurrent = tStart;
  std::vector<double> y(y0);
  std::vector<double> yp(yp0);
  std::vector<double> z(z0);

  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  // Checking the voltage values at extreme nodes - Infinite node and F21 bus
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[2], 0.947666);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[3], -0.0922538);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[10], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[11], 1);

  // Here we detect the algebraic mode change that occurs at t=2.
  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, true);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[2], 0.947666);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[3], -0.0922538);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[10], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[11], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->modeChangeAlg(), true);

  solver->reinit(y, yp, z);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[2], 0.926843);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[3], -0.120835);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[10], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[11], 1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(model->modeChangeAlg(), false);

  solver->solve(tStop, tCurrent, y, yp, z, algebraicMode);
  ASSERT_DOUBLE_EQUALS_DYNAWO(algebraicMode, false);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[2], 0.926843);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[3], -0.120835);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[10], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(y[11], 1);
}

}  // namespace DYN
