# rapidjson hasn't released a version with > 3.5
SET(CMAKE_POLICY_VERSION_MINIMUM 3.5)

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

PROJECT(pcbenv VERSION 1.0)

SET(LIB_NAME LibEnvPCB)

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED True)

CMAKE_POLICY(SET CMP0074 NEW)
CMAKE_POLICY(SET CMP0078 NEW)
CMAKE_POLICY(SET CMP0086 NEW)
CMAKE_POLICY(SET CMP0135 NEW)
CMAKE_POLICY(SET CMP0167 NEW)
CMAKE_POLICY(SET CMP0177 NEW)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)


# ======= #
# OPTIONS #
# --------#

OPTION(ENABLE_UI "Enable Qt/OpenGL-based UI." OFF)


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
    Py.cpp
    AABBTree.cpp
    AShape.cpp
    AShapeInexact.cpp
    Color.cpp
    Component.cpp
    Connection.cpp
    DRC.cpp
    Env.cpp
    EnvPCB.cpp
    Enums.cpp
    GridDirection.cpp
    Layer.cpp
    LayoutArea.cpp
    Log.cpp
    Math/Mat4.cpp
    NavGrid.cpp
    NavImage.cpp
    NavTriangulation.cpp
    Net.cpp
    Object.cpp
    Parameter.cpp
    Path.cpp
    PCBoard.cpp
    Pin.cpp
    Rules.cpp
    Signals.cpp
    Track.cpp
    UserSettings.cpp
    Util/Metrics.cpp
    Util/PCBItemSets.cpp
    Util/Util.cpp
    Via.cpp
    UI/Application.cpp
)
SET(SRC_FILES_PCB_LOADERS
    Loaders/DSNParser.cpp
    Loaders/DSNParserBase.cpp
    Loaders/Factory.cpp
    Loaders/JSONParser.cpp
    Loaders/KiCADParser.cpp
    Loaders/LoaderDSN.cpp
    Loaders/RouteTracker.cpp
)
SET(SRC_FILES_RL
    RL/Action.cpp
    RL/Actions/AddSegment.cpp
    RL/Actions/Grid.cpp
    RL/Actions/LayerMask.cpp
    RL/Actions/Route.cpp
    RL/Actions/RouteAStar.cpp
    RL/Actions/SetTrack.cpp
    RL/Actions/Unroute.cpp
    RL/ActionSpace.cpp
    RL/Agent.cpp
    RL/Policy.cpp
    RL/Reward.cpp
    RL/StateRepresentation.cpp
    RL/State/DRCheck.cpp
    RL/State/Endpoints.cpp
    RL/State/Features.cpp
    RL/State/Grid.cpp
    RL/State/ImageLike.cpp
    RL/State/Items.cpp
    RL/State/Metrics.cpp
    RL/State/Tracks.cpp
    RL/Stats.cpp
    RL/RRRAgent.cpp
    RL/UserAgent.cpp
)
SET(SRC_FILES_GUI_QT
    UI/Qt/QApp.cpp
)
SET(SRC_FILES_GUI_COMMON
    UI/Actions.cpp
    UI/ActionTab.cpp
    UI/BrowserTab.cpp
    UI/Camera.cpp
    UI/GLWidget.cpp
    UI/Helper.cpp
    UI/NavGrid.cpp
    UI/NavMesh.cpp
    UI/ParameterTab.cpp
    UI/PCBoardMesh.cpp
    UI/RatsNest.cpp
    UI/RoutePainterLines.cpp
    UI/RoutePainterPretty.cpp
    UI/Shaders.cpp
    UI/TriList.cpp
    UI/ViaPainter.cpp
    UI/ViewTab.cpp
    UI/Window.cpp
)
SET(HEADER_FILES_GUI_QT
    UI/Qt/QApp.hpp
    UI/Qt/Loader.hpp
    UI/ActionTab.hpp
    UI/BrowserTab.hpp
    UI/ParameterTab.hpp
    UI/ViewTab.hpp
    UI/Window.hpp
    ../data/ui/qt/action_tab.ui
    ../data/ui/qt/browser_tab.ui
    ../data/ui/qt/view_tab.ui
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

SET_SOURCE_FILES_PROPERTIES(Env.i PROPERTIES CPLUSPLUS ON)
SET_SOURCE_FILES_PROPERTIES(Env.i PROPERTIES SWIG_FLAGS "-includeall")

# NOTE: Qt headers define the keyword slots which is used in Python headers.
# ADD_DEFINITIONS(-DQT_NO_KEYWORDS)


# ================ #
# COMPILE AND LINK #
# ---------------- #

SWIG_ADD_LIBRARY(${LIB_NAME} LANGUAGE python SOURCES ${SRC_FILES_ALL} Env.i)
TARGET_LINK_LIBRARIES(${LIB_NAME} ${PYTHON_LIBRARIES} Python3::NumPy ${CMAKE_THREAD_LIBS_INIT})
IF(ENABLE_UI)
  TARGET_LINK_LIBRARIES(${LIB_NAME} Qt6::Widgets Qt6::UiTools)
ENDIF()
IF(OpenMP_CXX_FOUND)
  TARGET_LINK_LIBRARIES(${LIB_NAME} OpenMP::OpenMP_CXX)
ENDIF()

TARGET_INCLUDE_DIRECTORIES(${LIB_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}
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

TARGET_PRECOMPILE_HEADERS(${LIB_NAME} PUBLIC Geometry.hpp)

SET_SOURCE_FILES_PROPERTIES(AShapeInexact.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# ======= #
# INSTALL #
# ------- #
INSTALL(FILES ${CMAKE_BINARY_DIR}/_${LIB_NAME}.so ${CMAKE_BINARY_DIR}/EnvPCB.py DESTINATION "${CMAKE_SOURCE_DIR}/..")

