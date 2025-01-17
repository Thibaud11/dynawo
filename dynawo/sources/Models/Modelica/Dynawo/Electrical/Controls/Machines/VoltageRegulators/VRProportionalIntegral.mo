within Dynawo.Electrical.Controls.Machines.VoltageRegulators;

/*
* Copyright (c) 2015-2019, RTE (http://www.rte-france.com)
* See AUTHORS.txt
* All rights reserved.
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, you can obtain one at http://mozilla.org/MPL/2.0/.
* SPDX-License-Identifier: MPL-2.0
*
* This file is part of Dynawo, an hybrid C++/Modelica open source time domain simulation tool for power systems.
*/

model VRProportionalIntegral "Proportional Integral Voltage Regulator, keeps machine stator's voltage constant"

  import Modelica;

  import Dynawo.Connectors;
  import Dynawo.NonElectrical.Blocks.NonLinear.LimiterWithLag;
  import Dynawo.NonElectrical.Logs.Timeline;
  import Dynawo.NonElectrical.Logs.TimelineKeys;

  parameter Real Gain "Control gain";
  parameter Types.VoltageModulePu UsRefMaxPu "Maximum stator reference voltage in p.u (base UNom)";
  parameter Types.VoltageModulePu UsRefMinPu "Maximum stator reference voltage in p.u (base UNom)";
  parameter Types.VoltageModulePu EfdMinPu "Minimum allowed EfdPu";
  parameter Types.VoltageModulePu EfdMaxPu "Maximum allowed EfdPu";
  parameter Types.Time LagEfdMin "Time lag before taking action when going below EfdMin";
  parameter Types.Time LagEfdMax "Time lag before taking action when going above EfdMax";
  parameter Types.Time tIntegral "Time integration constant";

  LimiterWithLag limiterWithLag (UMin = EfdMinPu, UMax = EfdMaxPu, LagMin = LagEfdMin, LagMax = LagEfdMax, tUMinReached0 = tEfdMinReached0, tUMaxReached0 = tEfdMaxReached0) "Limiter activated only after a certain period outside the bounds" annotation(
    Placement(visible = true, transformation(origin = {80, 0}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Math.Gain gain (k = Gain) annotation(
    Placement(visible = true, transformation(origin = {-8, 0}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Math.Feedback feedback annotation(
    Placement(visible = true, transformation(origin = {-60, 0}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Interfaces.RealInput UsRefPu(start = UsRef0Pu) "General control voltage" annotation(
    Placement(visible = true, transformation(origin = {-142, 0}, extent = {{-20, -20}, {20, 20}}, rotation = 0), iconTransformation(origin = {-142, 0}, extent = {{-20, -20}, {20, 20}}, rotation = 0)));
  Modelica.Blocks.Interfaces.RealInput UsPu (start = Us0Pu) "Stator voltage" annotation(
    Placement(visible = true, transformation(origin = {-60, -48}, extent = {{-20, -20}, {20, 20}}, rotation = 90), iconTransformation(origin = {-56, -40}, extent = {{-20, -20}, {20, 20}}, rotation = 0)));
  Modelica.Blocks.Interfaces.RealOutput EfdPu (start = Efd0Pu) "Exciter field voltage" annotation(
    Placement(visible = true, transformation(origin = {120, 0}, extent = {{-10, -10}, {10, 10}}, rotation = 0), iconTransformation(origin = {120, 0}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Math.Add rawEfd annotation(
    Placement(visible = true, transformation(origin = {46, 0}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Continuous.Integrator integrator(y_start = yIntegrator0, k = Gain/tIntegral)  annotation(
    Placement(visible = true, transformation(origin = {12, 32}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Connectors.ImPin EfdPuPin(value(start = Efd0Pu)) "Exciter field voltage Pin";
  Connectors.BPin  limitationUp (value (start = false)) "Limitation up reached ?";
  Connectors.BPin  limitationDown (value (start = false)) "Limitation down reached ?";
  Modelica.Blocks.Math.Add integratorEntry annotation(
    Placement(visible = true, transformation(origin = {-28, 32}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Math.Feedback feedbackNonWindUp annotation(
    Placement(visible = true, transformation(origin = {40, 64}, extent = {{-10, 10}, {10, -10}}, rotation = 180)));
  Modelica.Blocks.Nonlinear.Limiter limUsRef(limitsAtInit = true, uMax = UsRefMaxPu, uMin = UsRefMinPu)  annotation(
    Placement(visible = true, transformation(origin = {-98, 0}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
protected

  parameter Types.VoltageModulePu UsRef0Pu  "Initial control voltage, p.u. = Unom";
  parameter Types.VoltageModulePu Us0Pu  "Initial stator voltage, p.u. = Unom";
  parameter Types.VoltageModulePu Efd0Pu "Initial Efd";
  parameter Types.PerUnit yIntegrator0 "Initial control before saturation";
  parameter Types.Time tEfdMaxReached0 "Initial time when the Efd went above EfdMax";
  parameter Types.Time tEfdMinReached0 "Initial time when the Efd went below EfdMin";


equation
  connect(limUsRef.y, feedback.u1) annotation(
    Line(points = {{-86, 0}, {-68, 0}, {-68, 0}, {-68, 0}}, color = {0, 0, 127}));
  connect(UsRefPu, limUsRef.u) annotation(
    Line(points = {{-142, 0}, {-110, 0}, {-110, 0}, {-110, 0}}, color = {0, 0, 127}));
  connect(EfdPuPin.value, EfdPu) annotation(
    Line);
  connect(limiterWithLag.y, EfdPu) annotation(
    Line(points = {{91, 0}, {119, 0}}, color = {0, 0, 127}));
  connect(limiterWithLag.y, feedbackNonWindUp.u1) annotation(
    Line(points = {{91, 0}, {99, 0}, {99, 64}, {47, 64}, {47, 64}}, color = {0, 0, 127}));
  connect(rawEfd.y, limiterWithLag.u) annotation(
    Line(points = {{57, 0}, {67, 0}}, color = {0, 0, 127}));
  connect(integrator.y, rawEfd.u1) annotation(
    Line(points = {{23, 32}, {30, 32}, {30, 6}, {34, 6}}, color = {0, 0, 127}));
  connect(gain.y, rawEfd.u2) annotation(
    Line(points = {{3, 0}, {29.5, 0}, {29.5, -6}, {34, -6}}, color = {0, 0, 127}));
  connect(rawEfd.y, feedbackNonWindUp.u2) annotation(
    Line(points = {{57, 0}, {59, 0}, {59, 46}, {39, 46}, {39, 56}, {39, 56}}, color = {0, 0, 127}));
  connect(feedbackNonWindUp.y, integratorEntry.u1) annotation(
    Line(points = {{31, 64}, {-47, 64}, {-47, 38}, {-39, 38}, {-39, 38}}, color = {0, 0, 127}));
  connect(integratorEntry.y, integrator.u) annotation(
    Line(points = {{-17, 32}, {-1, 32}, {-1, 32}, {-1, 32}}, color = {0, 0, 127}));
  connect(gain.u, feedback.y) annotation(
    Line(points = {{-20, 0}, {-51, 0}}, color = {0, 0, 127}));
  connect(feedback.y, integratorEntry.u2) annotation(
    Line(points = {{-51, 0}, {-49, 0}, {-49, 26}, {-41, 26}, {-41, 26}}, color = {0, 0, 127}));
  connect(UsPu, feedback.u2) annotation(
    Line(points = {{-60, -48}, {-60, -8}}, color = {0, 0, 127}));

//TimeLine
  when time - limiterWithLag.tUMinReached >= LagEfdMin then
// low limit
    Timeline.logEvent1(TimelineKeys.VRLimitationDownward);
    limitationDown.value = true;
  elsewhen time - limiterWithLag.tUMinReached < LagEfdMin and pre(limitationDown.value) then
    Timeline.logEvent1(TimelineKeys.VRBackToRegulation);
    limitationDown.value = false;
  end when;

  when time - limiterWithLag.tUMaxReached >= LagEfdMax then
// high limit
    Timeline.logEvent1(TimelineKeys.VRLimitationUpward);
    limitationUp.value = true;
  elsewhen time - limiterWithLag.tUMaxReached < LagEfdMax and pre(limitationUp.value) then
    Timeline.logEvent1(TimelineKeys.VRBackToRegulation);
    limitationUp.value = false;
  end when;

end VRProportionalIntegral;
