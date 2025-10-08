cmake_minimum_required(VERSION 3.16)

project(flounder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

set(FLNDR_COMMON_SRCS
  src/DetectorConstruction.cc
  src/ActionInitialization.cc
  src/PrimaryGeneratorAction.cc
  src/RootrackerPrimaryGenerator.cc
  src/PhotonCountActions.cc
  src/OpticalPropertiesLoader.cc
  src/OpticalInit.cc
  src/PhotonBudget.cc
  src/Digitizer.cc
  src/IO.cc
  src/PMTSD.cc
  src/RunAction.cc
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
target_link_libraries(flndr PRIVATE ${Geant4_LIBRARIES} ${ROOT_LIBRARIES} Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL Qt5::3DCore Qt5::3DExtras Qt5::3DRender ZLIB::ZLIB)
target_compile_features(flndr PRIVATE cxx_std_17)
target_compile_options(flndr PRIVATE -Wall -Wextra -Wpedantic)
