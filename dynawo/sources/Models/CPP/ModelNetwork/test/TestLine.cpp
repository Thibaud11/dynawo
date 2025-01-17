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

#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <IIDM/builders/NetworkBuilder.h>
#include <IIDM/builders/LineBuilder.h>
#include <IIDM/builders/VoltageLevelBuilder.h>
#include <IIDM/builders/SubstationBuilder.h>
#include <IIDM/builders/BusBuilder.h>
#include <IIDM/components/Line.h>
#include <IIDM/components/CurrentLimit.h>
#include <IIDM/components/VoltageLevel.h>
#include <IIDM/components/Bus.h>
#include <IIDM/components/Substation.h>
#include <IIDM/Network.h>

#include "DYNLineInterfaceIIDM.h"
#include "DYNVoltageLevelInterfaceIIDM.h"
#include "DYNCurrentLimitInterfaceIIDM.h"
#include "DYNBusInterfaceIIDM.h"
#include "DYNModelLine.h"
#include "DYNModelVoltageLevel.h"
#include "DYNModelBus.h"
#include "DYNModelNetwork.h"
#include "TLTimelineFactory.h"
#include "DYNSparseMatrix.h"
#include "DYNVariable.h"

#include "gtest_dynawo.h"

using boost::shared_ptr;

namespace DYN {
std::pair<shared_ptr<ModelLine>, shared_ptr<ModelVoltageLevel> >  // need to return the voltage level so that it is not destroyed
createModelLine(bool open, bool initModel, bool closed1 = true, bool closed2 = true) {
  IIDM::builders::NetworkBuilder nb;
  IIDM::Network networkIIDM = nb.build("MyNetwork");

  IIDM::builders::SubstationBuilder ssb;
  IIDM::Substation ss = ssb.build("MySubStation");
  IIDM::connection_status_t cs = {!open};
  IIDM::Port p1("MyBus1", cs), p2("MyBus2", cs);
  IIDM::Connection c1("MyVoltageLevel", p1, IIDM::side_1), c2("MyVoltageLevel", p2, IIDM::side_2);

  IIDM::builders::BusBuilder bb;
  IIDM::Bus bus1IIDM = bb.build("MyBus1");
  IIDM::Bus bus2IIDM = bb.build("MyBus2");

  IIDM::builders::VoltageLevelBuilder vlb;
  vlb.mode(IIDM::VoltageLevel::bus_breaker);
  vlb.nominalV(5.);
  IIDM::VoltageLevel vlIIDM = vlb.build("MyVoltageLevel");
  if (closed1)
    vlIIDM.add(bus1IIDM);
  if (closed2)
    vlIIDM.add(bus2IIDM);
  vlIIDM.lowVoltageLimit(0.5);
  vlIIDM.highVoltageLimit(2.);
  ss.add(vlIIDM);
  networkIIDM.add(ss);

  IIDM::builders::LineBuilder dlb;
  dlb.r(3.);
  dlb.x(3.);
  dlb.g1(3.);
  dlb.b1(3.);
  dlb.p1(105.);
  dlb.q1(90.);
  dlb.g2(6.);
  dlb.b2(1.5);
  dlb.p2(50.);
  dlb.q2(45.);
  IIDM::CurrentLimits limits(200.);
  limits.add("MyLimit", 10., 5.);
  limits.add("DeactivatedLimit", std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
  dlb.currentLimits1(limits);
  IIDM::CurrentLimits limits2(200.);
  limits2.add("MyLimit2", 15., 10.);
  dlb.currentLimits2(limits2);
  IIDM::Line dlIIDM = dlb.build("MyLine");
  if (closed1 && closed2)
    networkIIDM.add(dlIIDM, c1, c2);
  else if (closed1 && !closed2)
    networkIIDM.add(dlIIDM, c1, c1);
  else
    networkIIDM.add(dlIIDM, c2, c2);
  IIDM::Line dlIIDM2 = networkIIDM.get_line("MyLine");  // was copied...
  shared_ptr<LineInterfaceIIDM> dlItfIIDM = shared_ptr<LineInterfaceIIDM>(new LineInterfaceIIDM(dlIIDM2));
  shared_ptr<VoltageLevelInterfaceIIDM> vlItfIIDM = shared_ptr<VoltageLevelInterfaceIIDM>(new VoltageLevelInterfaceIIDM(vlIIDM));
  shared_ptr<BusInterfaceIIDM> bus1ItfIIDM;
  if (closed1)
    bus1ItfIIDM = shared_ptr<BusInterfaceIIDM>(new BusInterfaceIIDM(vlIIDM.get_bus("MyBus1")));
  shared_ptr<BusInterfaceIIDM> bus2ItfIIDM;
  if (closed2)
    bus2ItfIIDM = shared_ptr<BusInterfaceIIDM>(new BusInterfaceIIDM(vlIIDM.get_bus("MyBus2")));
  dlItfIIDM->setVoltageLevelInterface1(vlItfIIDM);
  if (closed1)
    dlItfIIDM->setBusInterface1(bus1ItfIIDM);
  dlItfIIDM->setVoltageLevelInterface2(vlItfIIDM);
  if (closed2)
    dlItfIIDM->setBusInterface2(bus2ItfIIDM);
  IIDM::CurrentLimits currentLimits = dlIIDM2.currentLimits1();
  for (IIDM::CurrentLimits::const_iterator it = currentLimits.begin(); it != currentLimits.end(); ++it) {
    shared_ptr<CurrentLimitInterfaceIIDM> cLimit(new CurrentLimitInterfaceIIDM(it->value, it->acceptableDuration));
    dlItfIIDM->addCurrentLimitInterface1(cLimit);
  }
  currentLimits = dlIIDM2.currentLimits2();
  for (IIDM::CurrentLimits::const_iterator it = currentLimits.begin(); it != currentLimits.end(); ++it) {
    shared_ptr<CurrentLimitInterfaceIIDM> cLimit(new CurrentLimitInterfaceIIDM(it->value, it->acceptableDuration));
    dlItfIIDM->addCurrentLimitInterface2(cLimit);
  }

  shared_ptr<ModelLine> dl = shared_ptr<ModelLine>(new ModelLine(dlItfIIDM));
  ModelNetwork* network = new ModelNetwork();
  network->setIsInitModel(initModel);
  network->setTimeline(timeline::TimelineFactory::newInstance("Test"));
  dl->setNetwork(network);
  shared_ptr<ModelVoltageLevel> vl = shared_ptr<ModelVoltageLevel>(new ModelVoltageLevel(vlItfIIDM));
  int offset = 0;
  if (closed1) {
    shared_ptr<ModelBus> bus1 = shared_ptr<ModelBus>(new ModelBus(bus1ItfIIDM));
    bus1->setNetwork(network);
    bus1->setVoltageLevel(vl);
    dl->setModelBus1(bus1);
    bus1->initSize();
    // There is a memory leak here, but whatever ...
    double* y1 = new double[bus1->sizeY()];
    double* yp1 = new double[bus1->sizeY()];
    double* f1 = new double[bus1->sizeF()];
    double* z1 = new double[bus1->sizeZ()];
    bus1->setReferenceZ(&z1[0], 0);
    bus1->setReferenceY(y1, yp1, f1, 0, 0);
    y1[ModelBus::urNum_] = 3.5;
    y1[ModelBus::uiNum_] = 2;
    bus1->init(offset);
  }
  if (closed2) {
    shared_ptr<ModelBus> bus2 = shared_ptr<ModelBus>(new ModelBus(bus2ItfIIDM));
    bus2->setNetwork(network);
    bus2->setVoltageLevel(vl);
    dl->setModelBus2(bus2);
    bus2->initSize();
    // There is a memory leak here, but whatever ...
    double* y2 = new double[bus2->sizeY()];
    double* yp2 = new double[bus2->sizeY()];
    double* f2 = new double[bus2->sizeF()];
    double* z2 = new double[bus2->sizeZ()];
    bus2->setReferenceZ(&z2[0], 0);
    bus2->setReferenceY(y2, yp2, f2, 0, 0);
    y2[ModelBus::urNum_] = 4.;
    y2[ModelBus::uiNum_] = 1.5;
    bus2->init(offset);
  }
  return std::make_pair(dl, vl);
}


TEST(ModelsModelNetwork, ModelNetworkLineInitializationClosed) {
  shared_ptr<ModelLine> dl = createModelLine(false, false).first;
  ASSERT_EQ(dl->id(), "MyLine");
  ASSERT_EQ(dl->getConnectionState(), CLOSED);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->getCurrentLimitsDesactivate(), 0.);
}

TEST(ModelsModelNetwork, ModelNetworkLineInitializationOpened) {
  shared_ptr<ModelLine> dl = createModelLine(true, false).first;
  ASSERT_EQ(dl->id(), "MyLine");
  ASSERT_EQ(dl->getConnectionState(), OPEN);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->getCurrentLimitsDesactivate(), 0.);
}

TEST(ModelsModelNetwork, ModelNetworkLineInitializationClosed2) {
  shared_ptr<ModelLine> dl = createModelLine(false, false, false).first;
  ASSERT_EQ(dl->id(), "MyLine");
  ASSERT_EQ(dl->getConnectionState(), CLOSED_2);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->getCurrentLimitsDesactivate(), 0.);

  dl->evalYMat();
}

TEST(ModelsModelNetwork, ModelNetworkLineInitializationClosed1) {
  shared_ptr<ModelLine> dl = createModelLine(false, false, true, false).first;
  ASSERT_EQ(dl->id(), "MyLine");
  ASSERT_EQ(dl->getConnectionState(), CLOSED_1);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->getCurrentLimitsDesactivate(), 0.);

  dl->evalYMat();
}

TEST(ModelsModelNetwork, ModelNetworkLineCalculatedVariables) {
  shared_ptr<ModelLine> dl = createModelLine(false, false).first;
  dl->initSize();
  std::vector<double> y(dl->sizeY(), 0.);
  std::vector<double> yp(dl->sizeY(), 0.);
  std::vector<double> f(dl->sizeF(), 0.);
  std::vector<double> z(dl->sizeZ(), 0.);
  dl->setReferenceZ(&z[0], 0);
  dl->setReferenceY(&y[0], &yp[0], &f[0], 0, 0);
  dl->evalYMat();
  ASSERT_EQ(dl->sizeCalculatedVar(), ModelLine::nbCalculatedVariables_);

  std::vector<double> calculatedVars(ModelLine::nbCalculatedVariables_, 0.);
  dl->setReferenceCalculatedVar(&calculatedVars[0], 0);
  dl->evalCalculatedVars();
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::i1Num_], 4.3158702611537239);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::i2Num_], 6.5816519477340263);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::p1Num_], 12.270833333333332);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::p2Num_], 27.3125);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::q1Num_], -12.333333333333336);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::q2Num_], -6.6770833333333321);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS1ToS2Side1Num_], 49835.377141292069);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS2ToS1Side1Num_], -49835.377141292069);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS1ToS2Side2Num_], -75998.37047473331);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS2ToS1Side2Num_], 75998.37047473331);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iSide1Num_], 49835.377141292069);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iSide2Num_], 75998.37047473331);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::u1Num_], 4.0311288741492746);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::u2Num_], 4.2720018726587652);
  ASSERT_EQ(calculatedVars[ModelLine::lineStateNum_],CLOSED);
  std::vector<double> yI(4, 0.);
  yI[0] = 3.5;
  yI[1] = 2;
  yI[2] = 4.;
  yI[3] = 1.5;
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::i1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::i1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::i2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::i2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::p1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::p1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::p2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::p2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::q1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::q1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::q2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::q2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS1ToS2Side1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS1ToS2Side1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS2ToS1Side1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS2ToS1Side1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS1ToS2Side2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS1ToS2Side2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS2ToS1Side2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS2ToS1Side2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iSide1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iSide1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iSide2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iSide2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::u1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::u1Num_]);
  yI[0] = 4.;
  yI[1] = 1.5;
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::u2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::u2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::lineStateNum_, &yI[0], &yp[0]), calculatedVars[ModelLine::lineStateNum_]);
  ASSERT_THROW_DYNAWO(dl->evalCalculatedVarI(42, &yI[0], &yp[0]), Error::MODELER, KeyError_t::UndefCalculatedVarI);

  yI[0] = 3.5;
  yI[1] = 2;
  yI[2] = 4.;
  yI[3] = 1.5;
  std::vector<double> res(4, 0.);
  ASSERT_THROW_DYNAWO(dl->evalJCalculatedVarI(42, &yI[0], &yp[0], res), Error::MODELER, KeyError_t::UndefJCalculatedVarI);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::i1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.89020606654237955);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.57965971165276509);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.029365134594484289);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], -0.051087288952047991);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS1ToS2Side1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 10279.214243049617);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 6693.3338112220972);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 339.07936725830194);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], -589.90520057266212);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS2ToS1Side1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], -10279.214243049617);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], -6693.3338112220972);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], -339.07936725830194);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 589.90520057266212);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iSide1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 10279.214243049617);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 6693.3338112220972);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 339.07936725830194);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], -589.90520057266212);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::i2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], -0.010946888666137453);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], -0.057899808728124606);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 1.4614755820417966);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.59324223156971401);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS1ToS2Side2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 126.40378236366647);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 668.5694031042118);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], -16875.666414117928);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], -6850.1712418285751);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS2ToS1Side2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], -126.40378236366647);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], -668.5694031042118);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 16875.666414117928);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 6850.1712418285751);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iSide2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], -126.40378236366647);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], -668.5694031042118);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 16875.666414117928);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 6850.1712418285751);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::p1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 5.3124999999999991);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 3.270833333333333);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], -0.062499999999999972);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], -0.22916666666666669);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::p2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], -0.10416666666666666);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], -0.22916666666666669);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 12.104166666666668);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 4.6875);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::q1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], -5.0625000000000009);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], -3.0625);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], -0.22916666666666669);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.062499999999999972);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::q2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], -0.22916666666666669);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.10416666666666666);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], -2.7291666666666661);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], -1.229166666666667);
  std::fill(res.begin(), res.end(), 0);
  res.resize(2);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::u1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.8682431421244593);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.49613893835683387);
  std::fill(res.begin(), res.end(), 0);
  yI[0] = 4.;
  yI[1] = 1.5;
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::u2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.93632917756904455);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.3511234415883917);
  std::fill(res.begin(), res.end(), 0);
  res.resize(0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::lineStateNum_, &yI[0], &yp[0], res));

  int offset = 4;
  dl->init(offset);
  std::vector<int> numVars;
  ASSERT_THROW_DYNAWO(dl->getDefJCalculatedVarI(42, numVars), Error::MODELER, KeyError_t::UndefJCalculatedVarI);
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::i1Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS1ToS2Side1Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS2ToS1Side1Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iSide1Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::i2Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS1ToS2Side2Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS2ToS1Side2Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iSide2Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::p1Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::p2Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::q1Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::q2Num_, numVars));
  ASSERT_EQ(numVars.size(), 4);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::u1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::u2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i+2);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::lineStateNum_, numVars));
  ASSERT_EQ(numVars.size(), 0);
  numVars.clear();

  shared_ptr<ModelLine> dlInit = createModelLine(false, true).first;
  dlInit->initSize();
  ASSERT_EQ(dlInit->sizeCalculatedVar(), 0);
}

TEST(ModelsModelNetwork, ModelNetworkLineCalculatedVariablesClosed2) {
  shared_ptr<ModelLine> dl = createModelLine(false, false, false).first;
  dl->initSize();
  std::vector<double> y(dl->sizeY(), 0.);
  std::vector<double> yp(dl->sizeY(), 0.);
  std::vector<double> f(dl->sizeF(), 0.);
  std::vector<double> z(dl->sizeZ(), 0.);
  dl->setReferenceZ(&z[0], 0);
  dl->setReferenceY(&y[0], &yp[0], &f[0], 0, 0);
  dl->evalYMat();
  ASSERT_EQ(dl->sizeCalculatedVar(), ModelLine::nbCalculatedVariables_);

  std::vector<double> calculatedVars(ModelLine::nbCalculatedVariables_, 0.);
  dl->setReferenceCalculatedVar(&calculatedVars[0], 0);
  dl->evalCalculatedVars();
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::i1Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::i2Num_], 6.7494951734299242);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::p1Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::p2Num_], 28.175192307692306);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::q1Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::q2Num_], -6.1277884615384632);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS1ToS2Side1Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS2ToS1Side1Num_], -0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS1ToS2Side2Num_], -77936.457105476948);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS2ToS1Side2Num_], 77936.457105476948);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iSide1Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iSide2Num_], 77936.457105476948);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::u1Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::u2Num_], 4.2720018726587652);
  ASSERT_EQ(calculatedVars[ModelLine::lineStateNum_],CLOSED_2);
  std::vector<double> yI(2, 0.);
  yI[0] = 4.;
  yI[1] = 1.5;
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::i1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::i1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::i2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::i2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::p1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::p1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::p2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::p2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::q1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::q1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::q2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::q2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS1ToS2Side1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS1ToS2Side1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS2ToS1Side1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS2ToS1Side1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS1ToS2Side2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS1ToS2Side2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS2ToS1Side2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS2ToS1Side2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iSide1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iSide1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iSide2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iSide2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::u1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::u1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::u2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::u2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::lineStateNum_, &yI[0], &yp[0]), calculatedVars[ModelLine::lineStateNum_]);
  ASSERT_THROW_DYNAWO(dl->evalCalculatedVarI(42, &yI[0], &yp[0]), Error::MODELER, KeyError_t::UndefCalculatedVarI);

  std::vector<double> res(4, 0.);
  ASSERT_THROW_DYNAWO(dl->evalJCalculatedVarI(42, &yI[0], &yp[0], res), Error::MODELER, KeyError_t::UndefJCalculatedVarI);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::i1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS1ToS2Side1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS2ToS1Side1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iSide1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::i2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 1.479341407875052);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.55475302795314463);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS1ToS2Side2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], -0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], -0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], -17081.963201200426);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], -6405.736200450161);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS2ToS1Side2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 17081.963201200426);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 6405.736200450161);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iSide2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 17081.963201200426);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 6405.736200450161);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::p1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::p2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 12.350769230769231);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 4.6315384615384616);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::q1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::q2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], -2.6861538461538466);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], -1.0073076923076929);
  std::fill(res.begin(), res.end(), 0);
  res.resize(2);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::u1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::u2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.93632917756904455);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.3511234415883917);
  std::fill(res.begin(), res.end(), 0);
  res.resize(0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::lineStateNum_, &yI[0], &yp[0], res));

  int offset = 2;
  dl->init(offset);
  std::vector<int> numVars;
  ASSERT_THROW_DYNAWO(dl->getDefJCalculatedVarI(42, numVars), Error::MODELER, KeyError_t::UndefJCalculatedVarI);
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::i1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS1ToS2Side1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS2ToS1Side1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iSide1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::i2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS1ToS2Side2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS2ToS1Side2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iSide2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::p1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::p2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::q1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::q2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::u1Num_, numVars));
  ASSERT_EQ(numVars.size(), 0);
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::u2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::lineStateNum_, numVars));
  ASSERT_EQ(numVars.size(), 0);
  numVars.clear();
}

TEST(ModelsModelNetwork, ModelNetworkLineCalculatedVariablesClosed1) {
  shared_ptr<ModelLine> dl = createModelLine(false, false, true, false).first;
  dl->initSize();
  std::vector<double> y(dl->sizeY(), 0.);
  std::vector<double> yp(dl->sizeY(), 0.);
  std::vector<double> f(dl->sizeF(), 0.);
  std::vector<double> z(dl->sizeZ(), 0.);
  dl->setReferenceZ(&z[0], 0);
  dl->setReferenceY(&y[0], &yp[0], &f[0], 0, 0);
  dl->evalYMat();
  ASSERT_EQ(dl->sizeCalculatedVar(), ModelLine::nbCalculatedVariables_);

  std::vector<double> calculatedVars(ModelLine::nbCalculatedVariables_, 0.);
  dl->setReferenceCalculatedVar(&calculatedVars[0], 0);
  dl->evalCalculatedVars();
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::i1Num_], 4.2894353098502478);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::i2Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::p1Num_], 12.872143230983951);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::p2Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::q1Num_], -11.545381193300766);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::q2Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS1ToS2Side1Num_], 49530.132616270537);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS2ToS1Side1Num_], -49530.132616270537);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS1ToS2Side2Num_], -0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iS2ToS1Side2Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iSide1Num_], 49530.132616270537);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::iSide2Num_], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::u1Num_], 4.0311288741492746);
  ASSERT_DOUBLE_EQUALS_DYNAWO(calculatedVars[ModelLine::u2Num_], 0.);
  ASSERT_EQ(calculatedVars[ModelLine::lineStateNum_],CLOSED_1);
  std::vector<double> yI(2, 0.);
  yI[0] = 3.5;
  yI[1] = 2;
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::i1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::i1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::i2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::i2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::p1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::p1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::p2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::p2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::q1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::q1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::q2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::q2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS1ToS2Side1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS1ToS2Side1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS2ToS1Side1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS2ToS1Side1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS1ToS2Side2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS1ToS2Side2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iS2ToS1Side2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iS2ToS1Side2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iSide1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iSide1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::iSide2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::iSide2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::u1Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::u1Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::u2Num_, &yI[0], &yp[0]), calculatedVars[ModelLine::u2Num_]);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->evalCalculatedVarI(ModelLine::lineStateNum_, &yI[0], &yp[0]), calculatedVars[ModelLine::lineStateNum_]);
  ASSERT_THROW_DYNAWO(dl->evalCalculatedVarI(42, &yI[0], &yp[0]), Error::MODELER, KeyError_t::UndefCalculatedVarI);

  std::vector<double> res(4, 0.);
  ASSERT_THROW_DYNAWO(dl->evalJCalculatedVarI(42, &yI[0], &yp[0], res), Error::MODELER, KeyError_t::UndefJCalculatedVarI);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::i1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.92387837442928422);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.52793049967387662);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS1ToS2Side1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 10668.028563504424);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 6096.016322002527);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS2ToS1Side1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], -10668.028563504424);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], -6096.016322002527);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], -0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], -0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iSide1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 10668.028563504424);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 6096.016322002527);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::i2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS1ToS2Side2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iS2ToS1Side2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::iSide2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::p1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 5.5449232379623172);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 3.1685275645498958);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::p2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::q1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], -4.9733949755757152);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], -2.8419399860432657);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::q2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[2], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[3], 0.);
  std::fill(res.begin(), res.end(), 0);
  res.resize(2);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::u1Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.8682431421244593);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.49613893835683387);
  std::fill(res.begin(), res.end(), 0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::u2Num_, &yI[0], &yp[0], res));
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[0], 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(res[1], 0.);
  std::fill(res.begin(), res.end(), 0);
  res.resize(0);
  ASSERT_NO_THROW(dl->evalJCalculatedVarI(ModelLine::lineStateNum_, &yI[0], &yp[0], res));

  int offset = 2;
  dl->init(offset);
  std::vector<int> numVars;
  ASSERT_THROW_DYNAWO(dl->getDefJCalculatedVarI(42, numVars), Error::MODELER, KeyError_t::UndefJCalculatedVarI);
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::i1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS1ToS2Side1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS2ToS1Side1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iSide1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::i2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS1ToS2Side2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iS2ToS1Side2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::iSide2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::p1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::p2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::q1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::q2Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::u1Num_, numVars));
  ASSERT_EQ(numVars.size(), 2);
  for (size_t i = 0; i < numVars.size(); ++i) {
    ASSERT_EQ(numVars[i], i);
  }
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::u2Num_, numVars));
  ASSERT_EQ(numVars.size(), 0);
  numVars.clear();
  ASSERT_NO_THROW(dl->getDefJCalculatedVarI(ModelLine::lineStateNum_, numVars));
  ASSERT_EQ(numVars.size(), 0);
  numVars.clear();
}

TEST(ModelsModelNetwork, ModelNetworkLineDiscreteVariables) {
  std::pair<shared_ptr<ModelLine>, shared_ptr<ModelVoltageLevel> > p = createModelLine(false, false);
  shared_ptr<ModelLine> dl = p.first;
  dl->initSize();
  unsigned nbZ = 2;
  unsigned nbG = 9;
  ASSERT_EQ(dl->sizeZ(), nbZ);
  ASSERT_EQ(dl->sizeG(), nbG);
  std::vector<double> y(dl->sizeY(), 0.);
  std::vector<double> yp(dl->sizeY(), 0.);
  std::vector<double> f(dl->sizeF(), 0.);
  std::vector<double> z(nbZ, 0.);
  std::vector<state_g> g(nbG, NO_ROOT);
  dl->setReferenceG(&g[0], 0);
  dl->setReferenceZ(&z[0], 0);
  dl->setReferenceY(&y[0], &yp[0], &f[0], 0, 0);
  dl->setCurrentLimitsDesactivate(10.);

  dl->getY0();
  ASSERT_EQ(dl->getConnectionState(), CLOSED);
  ASSERT_EQ(z[0], dl->getConnectionState());
  dl->setConnectionState(OPEN);
  dl->getY0();
  ASSERT_EQ(dl->getConnectionState(), OPEN);
  ASSERT_EQ(z[0], dl->getConnectionState());
  dl->setConnectionState(CLOSED);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->getCurrentLimitsDesactivate(), 10.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z[1], dl->getCurrentLimitsDesactivate());

  z[0] = OPEN;
  z[1] = 0.;
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->getConnectionState(), OPEN);
  ASSERT_EQ(z[0], OPEN);
  ASSERT_DOUBLE_EQUALS_DYNAWO(dl->getCurrentLimitsDesactivate(), 0.);
  ASSERT_DOUBLE_EQUALS_DYNAWO(z[1], dl->getCurrentLimitsDesactivate());
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(OPEN);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::NO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::NO_CHANGE);

  dl->setConnectionState(CLOSED_1);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED_2);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED_3);
  ASSERT_THROW_DYNAWO(dl->evalZ(0.), Error::MODELER, KeyError_t::NoThirdSide);

  dl->setConnectionState(UNDEFINED_STATE);
  ASSERT_THROW_DYNAWO(dl->evalZ(0.), Error::MODELER, KeyError_t::UnsupportedComponentState);

  z[0] = CLOSED;
  dl->setConnectionState(OPEN);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::NO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::NO_CHANGE);

  dl->setConnectionState(CLOSED_1);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED_2);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED_3);
  ASSERT_THROW_DYNAWO(dl->evalZ(0.), Error::MODELER, KeyError_t::NoThirdSide);

  dl->setConnectionState(UNDEFINED_STATE);
  ASSERT_THROW_DYNAWO(dl->evalZ(0.), Error::MODELER, KeyError_t::UnsupportedComponentState);

  z[0] = CLOSED_1;
  dl->setConnectionState(OPEN);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED_1);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::NO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::NO_CHANGE);

  dl->setConnectionState(CLOSED_2);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED_3);
  ASSERT_THROW_DYNAWO(dl->evalZ(0.), Error::MODELER, KeyError_t::NoThirdSide);

  dl->setConnectionState(UNDEFINED_STATE);
  ASSERT_THROW_DYNAWO(dl->evalZ(0.), Error::MODELER, KeyError_t::UnsupportedComponentState);

  z[0] = CLOSED_2;
  dl->setConnectionState(OPEN);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED_1);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::TOPO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::TOPO_CHANGE);

  dl->setConnectionState(CLOSED_2);
  ASSERT_EQ(dl->evalZ(0.), NetworkComponent::NO_CHANGE);
  ASSERT_EQ(dl->evalState(0.), NetworkComponent::NO_CHANGE);

  dl->setConnectionState(CLOSED_3);
  ASSERT_THROW_DYNAWO(dl->evalZ(0.), Error::MODELER, KeyError_t::NoThirdSide);

  dl->setConnectionState(UNDEFINED_STATE);
  ASSERT_THROW_DYNAWO(dl->evalZ(0.), Error::MODELER, KeyError_t::UnsupportedComponentState);

  ASSERT_EQ(dl->evalState(0.), NetworkComponent::NO_CHANGE);

  z[0] = UNDEFINED_STATE;
  dl->setConnectionState(CLOSED);
  ASSERT_THROW_DYNAWO(dl->evalZ(0.), Error::MODELER, KeyError_t::UndefinedComponentState);
  z[0] = CLOSED_3;
  ASSERT_THROW_DYNAWO(dl->evalZ(0.), Error::MODELER, KeyError_t::NoThirdSide);
  z[0] = CLOSED;

  std::map<int, std::string> gEquationIndex;
  dl->setGequations(gEquationIndex);
  ASSERT_EQ(gEquationIndex.size(), nbG);
  for (size_t i = 0; i < nbG; ++i) {
    ASSERT_TRUE(gEquationIndex.find(i) != gEquationIndex.end());
  }
  ASSERT_NO_THROW(dl->evalG(0.));
  ASSERT_DOUBLE_EQUALS_DYNAWO(g[0], ROOT_DOWN);
  ASSERT_DOUBLE_EQUALS_DYNAWO(g[1], ROOT_DOWN);
  ASSERT_DOUBLE_EQUALS_DYNAWO(g[2], ROOT_DOWN);
  ASSERT_DOUBLE_EQUALS_DYNAWO(g[3], ROOT_DOWN);
  ASSERT_DOUBLE_EQUALS_DYNAWO(g[4], ROOT_DOWN);
  ASSERT_DOUBLE_EQUALS_DYNAWO(g[5], ROOT_DOWN);
  ASSERT_DOUBLE_EQUALS_DYNAWO(g[6], ROOT_DOWN);
  ASSERT_DOUBLE_EQUALS_DYNAWO(g[7], ROOT_DOWN);
  ASSERT_DOUBLE_EQUALS_DYNAWO(g[8], ROOT_DOWN);

  shared_ptr<ModelLine> dlInit = createModelLine(false, true).first;
  dlInit->initSize();
  ASSERT_EQ(dlInit->sizeZ(), 0);
  ASSERT_EQ(dlInit->sizeG(), 0);
}

TEST(ModelsModelNetwork, ModelNetworkLineContinuousVariables) {
  std::pair<shared_ptr<ModelLine>, shared_ptr<ModelVoltageLevel> > p = createModelLine(false, false);
  shared_ptr<ModelLine> dl = p.first;
  dl->initSize();
  unsigned nbY = 0;
  unsigned nbF = 0;
  ASSERT_EQ(dl->sizeY(), nbY);
  ASSERT_EQ(dl->sizeF(), nbF);
  std::vector<double> y(nbY, 0.);
  std::vector<double> yp(nbY, 0.);
  std::vector<propertyContinuousVar_t> yTypes(nbY, UNDEFINED_PROPERTY);
  std::vector<double> f(nbF, 0.);
  std::vector<propertyF_t> fTypes(nbF, UNDEFINED_EQ);
  std::vector<double> z(dl->sizeZ(), 0.);
  dl->setReferenceZ(&z[0], 0);
  dl->setReferenceY(&y[0], &yp[0], &f[0], 0, 0);
  dl->evalYMat();
  dl->setBufferYType(&yTypes[0], 0);
  dl->setBufferFType(&fTypes[0], 0);

  // test evalYType
  ASSERT_NO_THROW(dl->evalYType());
  ASSERT_NO_THROW(dl->evalFType());

  // test getY0
  ASSERT_NO_THROW(dl->getY0());

  // test evalF
  ASSERT_NO_THROW(dl->evalF());

  // test setFequations
  std::map<int, std::string> fEquationIndex;
  dl->setFequations(fEquationIndex);
  ASSERT_TRUE(fEquationIndex.empty());

  shared_ptr<ModelLine> dlInit = createModelLine(false, true).first;
  dlInit->initSize();
  ASSERT_EQ(dlInit->sizeY(), 0);
  ASSERT_EQ(dlInit->sizeF(), 0);
  ASSERT_NO_THROW(dlInit->getY0());
  ASSERT_NO_THROW(dlInit->evalF());
  fEquationIndex.clear();
  ASSERT_NO_THROW(dlInit->setFequations(fEquationIndex));
}

TEST(ModelsModelNetwork, ModelNetworkLineDefineInstantiate) {
  std::pair<shared_ptr<ModelLine>, shared_ptr<ModelVoltageLevel> > p = createModelLine(false, false);
  shared_ptr<ModelLine> dl = p.first;

  std::vector<shared_ptr<Variable> > definedVariables;
  std::vector<shared_ptr<Variable> > instantiatedVariables;
  dl->defineVariables(definedVariables);
  dl->instantiateVariables(instantiatedVariables);
  ASSERT_EQ(definedVariables.size(), instantiatedVariables.size());

  for (size_t i = 0, iEnd = definedVariables.size(); i < iEnd; ++i) {
    std::string var = instantiatedVariables[i]->getName();
    boost::replace_all(var, dl->id(), "@ID@");
    ASSERT_EQ(definedVariables[i]->getName(), var);
    ASSERT_EQ(definedVariables[i]->getType(), instantiatedVariables[i]->getType());
  }


  std::vector<ParameterModeler> parameters;
  dl->defineNonGenericParameters(parameters);
  ASSERT_TRUE(parameters.empty());
  boost::unordered_map<std::string, ParameterModeler> parametersModels;
  const std::string paramName = "dangling_line_currentLimit_maxTimeOperation";
  ParameterModeler param = ParameterModeler(paramName, VAR_TYPE_DOUBLE, EXTERNAL_PARAMETER);
  param.setValue<double>(10., PAR);
  parametersModels.insert(std::make_pair(paramName, param));
  ASSERT_NO_THROW(dl->setSubModelParameters(parametersModels));
}

TEST(ModelsModelNetwork, ModelNetworkLineJt) {
  std::pair<shared_ptr<ModelLine>, shared_ptr<ModelVoltageLevel> > p = createModelLine(false, false);
  shared_ptr<ModelLine> dl = p.first;
  dl->initSize();
  SparseMatrix smj;
  int size = dl->sizeF();
  smj.init(size, size);
  dl->evalJt(smj, 1., 0);
  ASSERT_EQ(smj.nbElem(), 0);

  SparseMatrix smjPrime;
  smjPrime.init(size, size);
  dl->evalJtPrim(smjPrime, 0);
  ASSERT_EQ(smjPrime.nbElem(), 0);

  shared_ptr<ModelLine> dlInit = createModelLine(false, true).first;
  dlInit->initSize();
  SparseMatrix smjInit;
  smjInit.init(dlInit->sizeY(), dlInit->sizeY());
  ASSERT_NO_THROW(dlInit->evalJt(smjInit, 1., 0));
  ASSERT_EQ(smjInit.nbElem(), 0);
}


}  // namespace DYN
