cmake_minimum_required(VERSION 3.16)

# Top project name is cosmetic; targets carry the real meaning.
project(flounder LANGUAGES CXX)

# --- Global C++ standard (safe even if also set per-target)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find dependencies
# Geant4: request the components you actually use
find_package(Geant4 REQUIRED COMPONENTS gdml ui vis)
# Pull in Geant4 compile defs/includes (classic style)
include(${Geant4_USE_FILE})

# ZLIB (mac quirk override kept since you had it)
set(ZLIB_ROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr)
find_package(ZLIB REQUIRED)
set(ZLIB_VERSION_STRING "1.3.1")

# ROOT for Rootracker reading
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree)
set(ROOT_LIBRARIES ROOT::Core ROOT::RIO ROOT::Tree)

# --- Common sources used by all days
set(FLNDR_COMMON_SRCS
  src/DetectorConstruction.cc
  src/ActionInitialization.cc
  src/RootrackerPrimaryGenerator.cc
  src/PhotonCountActions.cc
  src/OpticalPropertiesLoader.cc
  src/OpticalInit.cc
  src/PhotonBudget.cc
)

# --- Common include path (headers live in project/include)
set(FLNDR_COMMON_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${ROOT_INCLUDE_DIRS}
)

# ---------------- Day 1 ----------------
add_executable(flndr_day1
  src/main.cc
  ${FLNDR_COMMON_SRCS}
)

target_include_directories(flndr_day1 PRIVATE ${FLNDR_COMMON_INCLUDES})
target_link_libraries(flndr_day1 PRIVATE ${Geant4_LIBRARIES} ${ROOT_LIBRARIES})
target_compile_features(flndr_day1 PRIVATE cxx_std_17)

# ---------------- Day 2 ----------------
add_executable(flndr_day2
  src/main_day2.cc        # If you don't have this, point to src/main.cc
  ${FLNDR_COMMON_SRCS}
)

target_include_directories(flndr_day2 PRIVATE ${FLNDR_COMMON_INCLUDES})
target_link_libraries(flndr_day2 PRIVATE ${Geant4_LIBRARIES} ${ROOT_LIBRARIES})
target_compile_features(flndr_day2 PRIVATE cxx_std_17)

# ---------------- Day 3 (Photon budget & first hit time) ----------------
add_executable(flndr_day3
  src/main_day2.cc        # Reuse Day-2 main unless you made a main_day3.cc
  ${FLNDR_COMMON_SRCS}
)

target_include_directories(flndr_day3 PRIVATE ${FLNDR_COMMON_INCLUDES})
target_link_libraries(flndr_day3 PRIVATE ${Geant4_LIBRARIES} ${ROOT_LIBRARIES})
target_compile_features(flndr_day3 PRIVATE cxx_std_17)

# (Optional) a bit of hygiene
foreach(tgt IN ITEMS flndr_day1 flndr_day2 flndr_day3)
  target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()
