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
#include <vector>

#include "DYNModelCurrentLimits.h"
#include "CSTRConstraintsCollectionFactory.h"
#include "CSTRConstraintsCollection.h"
#include "TLTimelineFactory.h"
#include "CSTRConstraint.h"
#include "DYNModelNetwork.h"
#include "gtest_dynawo.h"

namespace DYN {

TEST(ModelsModelNetwork, ModelNetworkCurrentLimits) {
  ModelCurrentLimits mcl;
  ASSERT_EQ(mcl.sizeZ(), 0);
  ASSERT_EQ(mcl.sizeG(), 0);

  mcl.setNbLimits(2);
  ASSERT_EQ(mcl.sizeZ(), 0);
  ASSERT_EQ(mcl.sizeG(), 6);
  double t = 0.;
  double current = 4.;
  std::vector<state_g> states(mcl.sizeG(), NO_ROOT);
  const double desactivate = 0.;


  boost::shared_ptr<constraints::ConstraintsCollection> constraints =
      constraints::ConstraintsCollectionFactory::newInstance("MyConstaintsCollection");
  ModelNetwork network;
  network.setConstraints(constraints);
  network.setTimeline(timeline::TimelineFactory::newInstance("Test"));

  EXPECT_DEATH(mcl.evalG("MY COMP", t, current, &states[0], desactivate), "Mismatching number of limits and vector sizes");
  mcl.addLimit(8.);
  mcl.addLimit(std::numeric_limits<int>::max());
  EXPECT_DEATH(mcl.evalG("MY COMP", t, current, &states[0], desactivate), "Mismatching number of limits and vector sizes");
  mcl.addAcceptableDuration(5);
  mcl.addAcceptableDuration(std::numeric_limits<int>::max());
  mcl.setMaxTimeOperation(10.);

  mcl.evalG("MY COMP", t, current, &states[0], desactivate);
  for (size_t i = 0; i < states.size(); ++i) {
    ASSERT_EQ(states[i], ROOT_DOWN);
  }
  current = 9.;
  mcl.evalG("MY COMP", t, current, &states[0], desactivate);
  for (size_t i = 0; i < states.size(); ++i) {
    if (i == 0)
      ASSERT_EQ(states[i], ROOT_UP);
    else if (i == 3)
      ASSERT_EQ(states[i], ROOT_UP);
    else
      ASSERT_EQ(states[i], ROOT_DOWN);
  }
  mcl.evalZ("MY COMP", t, &states[0], &network, desactivate);

  unsigned i = 0;
  for (constraints::ConstraintsCollection::const_iterator it = constraints->cbegin(),
      itEnd = constraints->cend(); it != itEnd; ++it) {
    boost::shared_ptr<constraints::Constraint> constraint = (*it);
    if (i == 0) {
      ASSERT_EQ(constraint->getModelName(), "MY COMP");
      ASSERT_EQ(constraint->getDescription(), "IMAP 0");
      ASSERT_DOUBLE_EQUALS_DYNAWO(constraint->getTime(), 0.);
      ASSERT_EQ(constraint->getType(), constraints::CONSTRAINT_BEGIN);
    } else if (i == 1) {
      ASSERT_EQ(constraint->getModelName(), "MY COMP");
      ASSERT_EQ(constraint->getDescription(), "OverloadUp 5 0");
      ASSERT_DOUBLE_EQUALS_DYNAWO(constraint->getTime(), 0.);
      ASSERT_EQ(constraint->getType(), constraints::CONSTRAINT_BEGIN);
    } else {
      assert(0);
    }
    ++i;
  }
  ASSERT_EQ(i, 2);
  current = 4.;
  t = 5.1;
  mcl.evalG("MY COMP", t, current, &states[0], desactivate);
  for (size_t i = 0; i < states.size(); ++i) {
    if (i == 0 || i ==3 || i == 4)
      ASSERT_EQ(states[i], ROOT_DOWN);
    else
      ASSERT_EQ(states[i], ROOT_UP);
  }
  network.setCurrentTime(5.1);
  mcl.evalZ("MY COMP", t, &states[0], &network, desactivate);

  i = 0;
  for (constraints::ConstraintsCollection::const_iterator it = constraints->cbegin(),
      itEnd = constraints->cend(); it != itEnd; ++it) {
    boost::shared_ptr<constraints::Constraint> constraint = (*it);
    if (i == 0) {
      ASSERT_EQ(constraint->getModelName(), "MY COMP");
      ASSERT_EQ(constraint->getDescription(), "OverloadOpen 5");
      ASSERT_DOUBLE_EQUALS_DYNAWO(constraint->getTime(), 5.1);
      ASSERT_EQ(constraint->getType(), constraints::CONSTRAINT_BEGIN);
    } else {
      assert(0);
    }
    ++i;
  }
  ASSERT_EQ(i, 1);
}
}  // namespace DYN
