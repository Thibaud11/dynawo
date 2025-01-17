# Copyright (c) 2015-2019, RTE (http://www.rte-france.com)
# See AUTHORS.txt
# All rights reserved.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, you can obtain one at http://mozilla.org/MPL/2.0/.
# SPDX-License-Identifier: MPL-2.0
#
# This file is part of Dynawo, an hybrid C++/Modelica open source time domain simulation tool for power systems.

# API version
SET(API_TL_VERSION_STRING ${DYNAWO_VERSION_STRING})
SET(API_TL_VERSION_MAJOR ${DYNAWO_VERSION_MAJOR})

SET(API_TL_SHARED
    xsd/timeline.xsd
    )

INSTALL(FILES ${API_TL_SHARED} DESTINATION ${SHAREDIR_NAME}/xsd/)

SET(API_TL_SOURCES
    TLTimeline.cpp
    TLTimelineImpl.cpp
    TLTimelineFactory.cpp
    TLEventImpl.cpp
    TLEventCmp.cpp
    TLEventFactory.cpp
    TLTxtExporter.cpp
    TLCsvExporter.cpp
    TLXmlExporter.cpp
    )

SET(API_TL_INCLUDE_HEADERS
    TLEvent.h
    TLEventFactory.h
    TLTimeline.h
    TLTimelineFactory.h
    TLExporter.h
    TLTxtExporter.h
    TLCsvExporter.h
    TLXmlExporter.h
    )

ADD_LIBRARY(dynawo_API_TL ${LIBRARY_TYPE} ${API_TL_SOURCES})

TARGET_INCLUDE_DIRECTORIES(dynawo_API_TL
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${INCLUDEDIR_NAME}>
    )

TARGET_LINK_LIBRARIES(dynawo_API_TL
    XMLSAXFormatter${LibXML_LINK_SUFFIX}
    dynawo_Common
    )

SET_TARGET_PROPERTIES(dynawo_API_TL PROPERTIES VERSION ${API_TL_VERSION_STRING}
                                             SOVERSION ${API_TL_VERSION_MAJOR})

INSTALL(TARGETS dynawo_API_TL EXPORT dynawo-targets DESTINATION ${LIBDIR_NAME})
INSTALL(FILES ${API_TL_INCLUDE_HEADERS} DESTINATION ${INCLUDEDIR_NAME})
