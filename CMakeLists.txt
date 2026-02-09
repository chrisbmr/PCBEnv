# rapidjson hasn't released a version with > 3.5
SET(CMAKE_POLICY_VERSION_MINIMUM 3.5)

CMAKE_MINIMUM_REQUIRED(VERSION 3.15)

PROJECT(pcbenv VERSION 1.0 LANGUAGES CXX)

SET(LIB_NAME LibEnvPCB)

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED True)

CMAKE_POLICY(SET CMP0074 NEW)
CMAKE_POLICY(SET CMP0078 NEW)
CMAKE_POLICY(SET CMP0086 NEW)
CMAKE_POLICY(SET CMP0135 NEW)
CMAKE_POLICY(SET CMP0167 NEW)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)


# ======= #
# OPTIONS #
# --------#

OPTION(ENABLE_UI "Enable Qt/OpenGL-based UI." ON)


# ============ #
# DEPENDENCIES #
# ------------ #

#SET(Python_ROOT_DIR "/path/to/python")
#SET(PYTHON_LIBRARY "/path/to/libpython3.so")
#SET(PYTHON_INCLUDE_DIR "/path/to/include/python3.*/")

#SET(Boost_INCLUDE_DIR "/path/to/boost/include")
#SET(CGAL_DIR "/path/to/CGAL")
#SET(RapidJSON_DIR "/path/to/rapidjson")

INCLUDE(FetchContent)

FetchContent_Declare(
  boost
  URL "https://archives.boost.io/release/1.87.0/source/boost_1_87_0.zip"
)
FetchContent_Declare(
  cgal
  URL "https://github.com/CGAL/cgal/releases/download/v6.1.1/CGAL-6.1.1.zip"
)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        11.0.2
)
FetchContent_Declare(
  rapidjson
  URL "https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.zip"
)
FetchContent_Declare(
  swig
  GIT_REPOSITORY https://github.com/swig/swig.git
  GIT_TAG v4.2.1
)


# ============= #
# BUILD OPTIONS #
# ------------- #

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
ENDIF()

SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_AUTOUIC ON)

IF(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
 SET(CMAKE_CXX_FLAGS_RELEASE "/Ox /arch:AVX2")
ELSE()
 SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -Wall -Wno-reorder")
ENDIF()

SET(CMAKE_SWIG_FLAGS "")


# ================== #
# CHECK DEPENDENCIES #
# ------------------ #

INCLUDE(CheckIncludeFileCXX)

CHECK_INCLUDE_FILE_CXX("format" STDFORMAT_AVAILABLE)
IF(NOT STDFORMAT_AVAILABLE)
  MESSAGE(STATUS "std::format not found, using fmt")
  FetchContent_MakeAvailable(fmt)
ELSE()
  MESSAGE(STATUS "std::format found")
ENDIF()

SET(Python3_FIND_VIRTUALENV ONLY)

FIND_PACKAGE(Python3 REQUIRED COMPONENTS NumPy)
FIND_PACKAGE(Threads)
FIND_PACKAGE(OpenMP)

# This does not work because we need SWIG's cmake commands before building (see https://github.com/swig/swig/pull/2646)
# FetchContent_MakeAvailable(swig)

FIND_PACKAGE(SWIG 4.0 COMPONENTS python)
IF(NOT SWIG_FOUND)
  SET(SWIG_EXECUTABLE "${PROJECT_SOURCE_DIR}/../../Dependencies/bin/swig")
  FIND_PACKAGE(SWIG 4.0 COMPONENTS python REQUIRED)
ENDIF()
INCLUDE(${SWIG_USE_FILE})

FIND_PACKAGE(RapidJSON)
IF(NOT RapidJSON_FOUND OR NOT EXISTS "${RAPIDJSON_INCLUDE_DIRS}")
  FetchContent_MakeAvailable(rapidjson)
ENDIF()
IF(NOT EXISTS "${RAPIDJSON_INCLUDE_DIRS}")
  INCLUDE_DIRECTORIES("${rapidjson_SOURCE_DIR}/include")
ENDIF()

FIND_PACKAGE(Boost COMPONENTS graph)
IF(NOT Boost_FOUND)
  SET(BOOST_INCLUDE_LIBRARIES graph)
  SET(BOOST_ENABLE_CMAKE ON)
  FetchContent_MakeAvailable(boost)
  MESSAGE(STATUS "Boost sources: " ${boost_SOURCE_DIR})
  # We don't need to FIND_PACKAGE again.
  SET(Boost_INCLUDE_DIR ${boost_SOURCE_DIR})
  #FIND_PACKAGE(Boost REQUIRED COMPONENTS graph)
  INCLUDE_DIRECTORIES(${boost_SOURCE_DIR})
ENDIF()

SET(CGAL_DISABLE_GMP ON)
FIND_PACKAGE(CGAL)
IF(NOT CGAL_FOUND)
  FetchContent_Populate(cgal)
  SET(CGAL_DIR "${cgal_SOURCE_DIR}")
  FIND_PACKAGE(CGAL REQUIRED)
ENDIF()

IF(ENABLE_UI)
  FIND_PACKAGE(Qt6 COMPONENTS Widgets UiTools)
  IF(Qt6_FOUND)
    SET_TARGET_PROPERTIES(Qt6::Core PROPERTIES MAP_IMPORTED_CONFIG_COVERAGE "RELEASE")
    ADD_DEFINITIONS(-DGYM_PCB_ENABLE_UI)
  ELSE()
    MESSAGE(WARNING "⚠ UI requested but Qt6 not found, disabling UI.")
    SET(ENABLE_UI OFF CACHE BOOL "" FORCE)
  ENDIF()
ENDIF()


# ============ #
# SOURCE FILES #
# ------------ #

SET(SRC_FILES_PCB
    pcbenv/cxx/Py.cpp
    pcbenv/cxx/AABBTree.cpp
    pcbenv/cxx/AShape.cpp
    pcbenv/cxx/AShapeInexact.cpp
    pcbenv/cxx/Color.cpp
    pcbenv/cxx/Component.cpp
    pcbenv/cxx/Connection.cpp
    pcbenv/cxx/DRC.cpp
    pcbenv/cxx/Env.cpp
    pcbenv/cxx/EnvPCB.cpp
    pcbenv/cxx/Enums.cpp
    pcbenv/cxx/GridDirection.cpp
    pcbenv/cxx/Layer.cpp
    pcbenv/cxx/LayoutArea.cpp
    pcbenv/cxx/Log.cpp
    pcbenv/cxx/Math/Mat4.cpp
    pcbenv/cxx/NavGrid.cpp
    pcbenv/cxx/NavImage.cpp
    pcbenv/cxx/NavTriangulation.cpp
    pcbenv/cxx/Net.cpp
    pcbenv/cxx/Object.cpp
    pcbenv/cxx/Parameter.cpp
    pcbenv/cxx/Path.cpp
    pcbenv/cxx/PCBoard.cpp
    pcbenv/cxx/Pin.cpp
    pcbenv/cxx/Rules.cpp
    pcbenv/cxx/Signals.cpp
    pcbenv/cxx/Track.cpp
    pcbenv/cxx/UserSettings.cpp
    pcbenv/cxx/Util/Metrics.cpp
    pcbenv/cxx/Util/PCBItemSets.cpp
    pcbenv/cxx/Util/Util.cpp
    pcbenv/cxx/Via.cpp
    pcbenv/cxx/UI/Application.cpp
)
SET(SRC_FILES_PCB_LOADERS
    pcbenv/cxx/Loaders/DSNParser.cpp
    pcbenv/cxx/Loaders/DSNParserBase.cpp
    pcbenv/cxx/Loaders/Factory.cpp
    pcbenv/cxx/Loaders/JSONParser.cpp
    pcbenv/cxx/Loaders/KiCADParser.cpp
    pcbenv/cxx/Loaders/LoaderDSN.cpp
    pcbenv/cxx/Loaders/RouteTracker.cpp
)
SET(SRC_FILES_RL
    pcbenv/cxx/RL/Action.cpp
    pcbenv/cxx/RL/Actions/AddSegment.cpp
    pcbenv/cxx/RL/Actions/Grid.cpp
    pcbenv/cxx/RL/Actions/LayerMask.cpp
    pcbenv/cxx/RL/Actions/Route.cpp
    pcbenv/cxx/RL/Actions/RouteAStar.cpp
    pcbenv/cxx/RL/Actions/SetTrack.cpp
    pcbenv/cxx/RL/Actions/Unroute.cpp
    pcbenv/cxx/RL/ActionSpace.cpp
    pcbenv/cxx/RL/Agent.cpp
    pcbenv/cxx/RL/Policy.cpp
    pcbenv/cxx/RL/Reward.cpp
    pcbenv/cxx/RL/StateRepresentation.cpp
    pcbenv/cxx/RL/State/DRCheck.cpp
    pcbenv/cxx/RL/State/Endpoints.cpp
    pcbenv/cxx/RL/State/Features.cpp
    pcbenv/cxx/RL/State/Grid.cpp
    pcbenv/cxx/RL/State/ImageLike.cpp
    pcbenv/cxx/RL/State/Items.cpp
    pcbenv/cxx/RL/State/Metrics.cpp
    pcbenv/cxx/RL/State/Tracks.cpp
    pcbenv/cxx/RL/Stats.cpp
    pcbenv/cxx/RL/RRRAgent.cpp
    pcbenv/cxx/RL/UserAgent.cpp
)
SET(SRC_FILES_GUI_QT
    pcbenv/cxx/UI/Qt/QApp.cpp
)
SET(SRC_FILES_GUI_COMMON
    pcbenv/cxx/UI/Actions.cpp
    pcbenv/cxx/UI/ActionTab.cpp
    pcbenv/cxx/UI/BrowserTab.cpp
    pcbenv/cxx/UI/Camera.cpp
    pcbenv/cxx/UI/GLWidget.cpp
    pcbenv/cxx/UI/Helper.cpp
    pcbenv/cxx/UI/NavGrid.cpp
    pcbenv/cxx/UI/NavMesh.cpp
    pcbenv/cxx/UI/ParameterTab.cpp
    pcbenv/cxx/UI/PCBoardMesh.cpp
    pcbenv/cxx/UI/RatsNest.cpp
    pcbenv/cxx/UI/RoutePainterLines.cpp
    pcbenv/cxx/UI/RoutePainterPretty.cpp
    pcbenv/cxx/UI/Shaders.cpp
    pcbenv/cxx/UI/TriList.cpp
    pcbenv/cxx/UI/ViaPainter.cpp
    pcbenv/cxx/UI/ViewTab.cpp
    pcbenv/cxx/UI/Window.cpp
)
SET(HEADER_FILES_GUI_QT
    pcbenv/cxx/UI/Qt/QApp.hpp
    pcbenv/cxx/UI/Qt/Loader.hpp
    pcbenv/cxx/UI/ActionTab.hpp
    pcbenv/cxx/UI/BrowserTab.hpp
    pcbenv/cxx/UI/ParameterTab.hpp
    pcbenv/cxx/UI/ViewTab.hpp
    pcbenv/cxx/UI/Window.hpp
    pcbenv/data/ui/qt/action_tab.ui
    pcbenv/data/ui/qt/browser_tab.ui
    pcbenv/data/ui/qt/view_tab.ui
)
IF(NOT ENABLE_UI)
  SET(SRC_FILES_GUI)
ELSE()
  SET(SRC_FILES_GUI ${SRC_FILES_GUI_QT} ${SRC_FILES_GUI_COMMON} ${HEADER_FILES_GUI_QT})
ENDIF()
SET(SRC_FILES_ALL
    ${SRC_FILES_PCB}
    ${SRC_FILES_PCB_LOADERS}
    ${SRC_FILES_RL}
    ${SRC_FILES_GUI}
)

SET_SOURCE_FILES_PROPERTIES(pcbenv/cxx/Env.i PROPERTIES
  CPLUSPLUS ON
  SWIG_FLAGS "-includeall")

# NOTE: Qt headers define the keyword slots which is used in Python headers.
# ADD_DEFINITIONS(-DQT_NO_KEYWORDS)


# ================ #
# COMPILE AND LINK #
# ---------------- #

SWIG_ADD_LIBRARY(${LIB_NAME}
  TYPE MODULE
  LANGUAGE python
  SOURCES ${SRC_FILES_ALL} pcbenv/cxx/Env.i)

IF(WIN32)
  SET_PROPERTY(TARGET ${LIB_NAME} PROPERTY SUFFIX "${Python_SOABI}.pyd")
ELSE()
  SET_PROPERTY(TARGET ${LIB_NAME} PROPERTY SUFFIX "${Python_SOABI}${CMAKE_SHARED_MODULE_SUFFIX}")
ENDIF()

TARGET_LINK_LIBRARIES(${LIB_NAME} ${PYTHON_LIBRARIES} Python3::NumPy ${CMAKE_THREAD_LIBS_INIT})
IF(ENABLE_UI)
  TARGET_LINK_LIBRARIES(${LIB_NAME} Qt6::Widgets Qt6::UiTools)
ENDIF()
IF(OpenMP_CXX_FOUND)
  TARGET_LINK_LIBRARIES(${LIB_NAME} OpenMP::OpenMP_CXX)
ENDIF()

MESSAGE(STATUS "Copying ${LIB_NAME} to ${CMAKE_CURRENT_BINARY_DIR}/pcbenv")

SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pcbenv")

TARGET_INCLUDE_DIRECTORIES(${LIB_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/pcbenv/cxx/
    ${PYTHON_INCLUDE_PATH}
    ${Python3_NumPy_INCLUDE_DIRS}
    ${CGAL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${RAPIDJSON_INCLUDE_DIRS}
)
IF(ENABLE_UI)
  TARGET_INCLUDE_DIRECTORIES(${LIB_NAME} PUBLIC ${Qt6Widgets_INCLUDE_DIRS})
ENDIF()

IF(NOT STDFORMAT_AVAILABLE)
  ADD_DEFINITIONS(-DGYM_PCB_NO_STDFORMAT)
  INCLUDE_DIRECTORIES(fmt)
  TARGET_LINK_LIBRARIES(${LIB_NAME} fmt::fmt)
ENDIF()

TARGET_PRECOMPILE_HEADERS(${LIB_NAME} PUBLIC pcbenv/cxx/Geometry.hpp)

SET_SOURCE_FILES_PROPERTIES(AShapeInexact.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# ======= #
# INSTALL #
# ------- #

INSTALL(TARGETS ${LIB_NAME} DESTINATION pcbenv RENAME _LibEnvPCB.so)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/EnvPCB.py DESTINATION pcbenv)
