cmake_minimum_required(VERSION 3.16...3.30)

project(flounder LANGUAGES CXX)

if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")
endif()

execute_process(
  COMMAND git rev-parse --short=12 HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE FLNDR_GIT_SHA
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT FLNDR_GIT_SHA)
  set(FLNDR_GIT_SHA "unknown")
endif()

set(FLNDR_COMPILER_STRING "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
string(REPLACE "\"" "\\\"" FLNDR_COMPILER_STRING "${FLNDR_COMPILER_STRING}")

set(FLNDR_CXX_FLAGS_STRING "${CMAKE_CXX_FLAGS}")
string(REPLACE "\"" "\\\"" FLNDR_CXX_FLAGS_STRING "${FLNDR_CXX_FLAGS_STRING}")

add_compile_definitions(
  FLNDR_GIT_SHA="${FLNDR_GIT_SHA}"
  FLNDR_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
  FLNDR_COMPILER="${FLNDR_COMPILER_STRING}"
  FLNDR_CXX_FLAGS="${FLNDR_CXX_FLAGS_STRING}"
)

# Prefer Homebrew Zlib when present, but honour explicit environment overrides.
if(NOT ZLIB_ROOT AND DEFINED ENV{ZLIB_ROOT} AND NOT "$ENV{ZLIB_ROOT}" STREQUAL "")
  set(ZLIB_ROOT "$ENV{ZLIB_ROOT}")
endif()
if(NOT ZLIB_ROOT)
  foreach(_zlib_prefix "/usr/local/opt/zlib" "/opt/homebrew/opt/zlib")
    if(EXISTS "${_zlib_prefix}")
      set(ZLIB_ROOT "${_zlib_prefix}")
      break()
    endif()
  endforeach()
endif()
if(ZLIB_ROOT)
  set(ZLIB_ROOT "${ZLIB_ROOT}" CACHE PATH "Preferred Zlib installation prefix" FORCE)
endif()
find_package(ZLIB 1.3.1 REQUIRED)

# Qt5 hints (Geant4's Qt UI/vis requires these modules).
if(NOT Qt5_DIR AND DEFINED ENV{Qt5_DIR} AND NOT "$ENV{Qt5_DIR}" STREQUAL "")
  set(Qt5_DIR "$ENV{Qt5_DIR}")
endif()
if(NOT Qt5_DIR)
  foreach(_qt_prefix "/usr/local/opt/qt@5" "/opt/homebrew/opt/qt@5" "/usr/local/opt/qt")
    if(EXISTS "${_qt_prefix}/lib/cmake/Qt5")
      set(Qt5_DIR "${_qt_prefix}/lib/cmake/Qt5")
      break()
    endif()
  endforeach()
endif()
if(Qt5_DIR)
  set(Qt5_DIR "${Qt5_DIR}" CACHE PATH "Qt5 configuration directory" FORCE)
  get_filename_component(_qt5_prefix "${Qt5_DIR}" DIRECTORY) # -> /.../lib/cmake
  get_filename_component(_qt5_prefix "${_qt5_prefix}" DIRECTORY) # -> /.../lib
  get_filename_component(_qt5_prefix "${_qt5_prefix}" DIRECTORY) # -> prefix
  list(APPEND CMAKE_PREFIX_PATH "${_qt5_prefix}")
endif()
find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL 3DCore 3DExtras 3DRender REQUIRED)

# Geant4 hint from local install
if(NOT Geant4_DIR AND DEFINED ENV{G4INSTALL})
  set(_g4_candidate "$ENV{G4INSTALL}/lib/cmake/Geant4")
  if(EXISTS "${_g4_candidate}/Geant4Config.cmake")
    set(Geant4_DIR "${_g4_candidate}" CACHE PATH "Geant4 configuration directory" FORCE)
  endif()
  unset(_g4_candidate)
endif()

find_package(Geant4 REQUIRED COMPONENTS gdml)
include(${Geant4_USE_FILE})

find_package(ROOT REQUIRED COMPONENTS Core RIO Tree)
set(ROOT_LIBRARIES ROOT::Core ROOT::RIO ROOT::Tree)

include(FetchContent)

find_package(yaml-cpp QUIET)

if (NOT yaml-cpp_FOUND)
  message(STATUS "yaml-cpp not found; fetching via FetchContent")
  set(YAML_CPP_BUILD_CONTRIB OFF CACHE INTERNAL "")
  set(YAML_CPP_BUILD_TOOLS OFF CACHE INTERNAL "")
  set(YAML_BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
  FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.7.0
  )
  FetchContent_GetProperties(yaml-cpp)
  if (NOT yaml-cpp_POPULATED)
    FetchContent_Populate(yaml-cpp)
    if (EXISTS "${yaml-cpp_SOURCE_DIR}/CMakeLists.txt")
      file(READ "${yaml-cpp_SOURCE_DIR}/CMakeLists.txt" YAML_CPP_CMAKELISTS)
      string(REPLACE "cmake_minimum_required(VERSION 3.4)"
                     "cmake_minimum_required(VERSION 3.5)"
                     YAML_CPP_CMAKELISTS
                     "${YAML_CPP_CMAKELISTS}")
      file(WRITE "${yaml-cpp_SOURCE_DIR}/CMakeLists.txt" "${YAML_CPP_CMAKELISTS}")
    endif()
    add_subdirectory(${yaml-cpp_SOURCE_DIR} ${yaml-cpp_BINARY_DIR})
  endif()
endif()

if (TARGET yaml-cpp::yaml-cpp)
  set(FLNDR_YAML_TARGET yaml-cpp::yaml-cpp)
elseif (TARGET yaml-cpp)
  set(FLNDR_YAML_TARGET yaml-cpp)
else()
  message(FATAL_ERROR "Failed to locate or fetch yaml-cpp target.")
endif()

set(FLNDR_COMMON_SRCS
  src/DetectorConstruction.cc
  src/ActionInitialization.cc
  src/PrimaryGeneratorAction.cc
  src/RootrackerPrimaryGenerator.cc
  src/PhotonCountActions.cc
  src/OpticalPropertiesLoader.cc
  src/OpticalProperties.cc
  src/OpticalInit.cc
  src/PhysicsList.cc
  src/PMTHit.cc
  src/RunManifest.cc
  src/PMTDigitizer.cc
  src/PhotonBudget.cc
  src/Digitizer.cc
  src/IO.cc
  src/PMTSD.cc
  src/RunAction.cc
  src/GeometryRegistry.cc
)

set(FLNDR_COMMON_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${ROOT_INCLUDE_DIRS}
)

add_executable(flndr
  src/main.cc
  ${FLNDR_COMMON_SRCS}
)

target_include_directories(flndr PRIVATE ${FLNDR_COMMON_INCLUDES})
target_link_libraries(flndr PRIVATE ${Geant4_LIBRARIES} ${ROOT_LIBRARIES} Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL Qt5::3DCore Qt5::3DExtras Qt5::3DRender ZLIB::ZLIB ${FLNDR_YAML_TARGET})
target_compile_features(flndr PRIVATE cxx_std_17)
target_compile_options(flndr PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if (BUILD_TESTING)
  add_executable(test_timing
    tests/test_timing.cc
    src/Digitizer.cc
    src/RunManifest.cc
  )
  target_include_directories(test_timing PRIVATE ${FLNDR_COMMON_INCLUDES})
  target_link_libraries(test_timing PRIVATE ${Geant4_LIBRARIES} ${ROOT_LIBRARIES} Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL Qt5::3DCore Qt5::3DExtras Qt5::3DRender ZLIB::ZLIB ${FLNDR_YAML_TARGET})
  add_test(NAME timing COMMAND test_timing)
endif()
add_executable(test_light_yield
  tests/test_light_yield.cc
  ${FLNDR_COMMON_SRCS}
)
target_include_directories(test_light_yield PRIVATE ${FLNDR_COMMON_INCLUDES})
target_compile_definitions(test_light_yield PRIVATE FLNDR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_light_yield PRIVATE ${Geant4_LIBRARIES} ${ROOT_LIBRARIES} ${FLNDR_YAML_TARGET})

add_test(NAME light_yield COMMAND test_light_yield)

add_test(NAME qc_qe_sweep_ratio
  COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_qe_sweep_ratio.sh)

add_test(NAME qc_timing_burst_sigma
  COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_timing_burst_sigma.sh)

add_test(NAME qc_dark_rate_poisson
  COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_dark_rate_poisson.sh)
