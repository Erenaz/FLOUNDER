# CMake generated Testfile for 
# Source directory: /Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/detector
# Build directory: /Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/detector/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[light_yield]=] "/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/detector/build/test_light_yield")
set_tests_properties([=[light_yield]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/detector/CMakeLists.txt;175;add_test;/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/detector/CMakeLists.txt;0;")
add_test([=[timing]=] "/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/detector/build/test_timing")
set_tests_properties([=[timing]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/detector/CMakeLists.txt;183;add_test;/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/detector/CMakeLists.txt;0;")
subdirs("_deps/yaml-cpp-build")
