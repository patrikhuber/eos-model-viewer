cmake_minimum_required(VERSION 3.1.3)
project(eos-model-viewer)
set(eos-model-viewer_VERSION 0.1.0.alpha)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# This sets the C++ standard to c++14 and required for all the following targets that we define.
# It has no effect on MSVC though - we thus define more specific requirements for each executable target respectively.
# Also it will not apply to the eos library target, since it is an INTERFACE_LIBRARY, and these properties do not apply to interface libraries.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # This makes CMake use -std=c++11 instead of -std=gnu++11
# This list is likely not complete, but it should be sufficient to error out on old compilers that we cannot build on:
set(eos-model-viewer_CXX_COMPILE_FEATURES cxx_defaulted_functions cxx_generalized_initializers cxx_generic_lambdas cxx_lambdas cxx_nonstatic_member_init cxx_range_for cxx_right_angle_brackets cxx_strong_enums)

# Build a CPack driven installer package:
include(InstallRequiredSystemLibraries) # This module will include any runtime libraries that are needed by the project for the current platform
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${eos-model-viewer_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${eos-model-viewer_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${eos-model-viewer_VERSION_PATCH}")
include(CPack)

# Find dependencies:
find_package(OpenCV REQUIRED core)
#check installed version in order to include the correct OpenCV libraries
#version variable is defined from project root's CMakeLists
if("${OpenCV_VERSION_MAJOR}$" EQUAL 2)
  message(STATUS "OpenCV 2.x detected")
  find_package(OpenCV 2.4.3 REQUIRED core imgproc highgui)
elseif("${OpenCV_VERSION_MAJOR}$" EQUAL 3)
  message(STATUS "OpenCV 3.x detected - including imgcodecs for compatibility")
  find_package(OpenCV 3 REQUIRED core imgproc imgcodecs)
endif()
# This allows us to compile in RelWithDebInfo. It'll use the Release-version of OpenCV:
set_target_properties(${OpenCV_LIBS} PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE)

# Include eos. Todo: We should really rather use an import target approach rather than running add_subdirectory here.
set(eos_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/eos")
set(EOS_BUILD_EXAMPLES OFF)
add_subdirectory(${eos_DIR})

######### The following will find libigl, and build everything that's required for the viewer - e.g. GLFW:
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(LIBIGL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/libigl/include")

option(LIBIGL_USE_STATIC_LIBRARY "Use libigl as static library" OFF)
option(LIBIGL_WITH_ANTTWEAKBAR       "Use AntTweakBar"    OFF)
option(LIBIGL_WITH_CGAL              "Use CGAL"           OFF)
option(LIBIGL_WITH_COMISO            "Use CoMiso"         OFF)
option(LIBIGL_WITH_CORK              "Use Cork"           OFF)
option(LIBIGL_WITH_EMBREE            "Use Embree"         OFF)
option(LIBIGL_WITH_LIM               "Use LIM"            OFF)
option(LIBIGL_WITH_MATLAB            "Use Matlab"         OFF)
option(LIBIGL_WITH_MOSEK             "Use MOSEK"          OFF)
option(LIBIGL_WITH_OPENGL            "Use OpenGL"         ON)
option(LIBIGL_WITH_OPENGL_GLFW       "Use GLFW"           ON)
option(LIBIGL_WITH_OPENGL_GLFW_IMGUI "Use ImGui"          ON)
option(LIBIGL_WITH_PNG               "Use PNG"            OFF)
option(LIBIGL_WITH_PYTHON            "Use Python"         OFF)
option(LIBIGL_WITH_TETGEN            "Use Tetgen"         OFF)
option(LIBIGL_WITH_TRIANGLE          "Use Triangle"       OFF)
option(LIBIGL_WITH_VIEWER            "Use OpenGL viewer"  ON)
option(LIBIGL_WITH_XML               "Use XML"            OFF)

find_package(LIBIGL REQUIRED QUIET)
######### libigl should be set up by now.

# Set up the eos-model-viewer target:
add_executable(eos-model-viewer eos-model-viewer.cpp cxxopts.hpp)
#target_include_directories(eos-model-viewer PRIVATE ${LIBIGL_INCLUDE_DIRS})
#add_definitions(${LIBIGL_DEFINITIONS})
target_compile_features(eos-model-viewer PRIVATE ${eos-model-viewer_CXX_COMPILE_FEATURES})
target_link_libraries(eos-model-viewer eos ${OpenCV_LIBS} igl::core igl::opengl_glfw igl::opengl_glfw_imgui)
target_link_libraries(eos-model-viewer "$<$<CXX_COMPILER_ID:GNU>:-pthread>$<$<CXX_COMPILER_ID:Clang>:-pthreads>")

# Install the binary:
install(TARGETS eos-model-viewer DESTINATION bin)
