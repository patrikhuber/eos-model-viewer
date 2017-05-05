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

if(MSVC)
  # The standard find_package for boost on Win finds the dynamic libs, so for dynamic linking to boost we need to #define:
  add_definitions(-DBOOST_ALL_NO_LIB) # Don't use the automatic library linking by boost with VS (#pragma ...). Instead, we specify everything here in cmake.
  add_definitions(-DBOOST_ALL_DYN_LINK) # Link against the dynamic boost lib - needs to match with the version that find_package finds.
endif()
find_package(Boost 1.50.0 COMPONENTS system filesystem program_options REQUIRED)
message(STATUS "Boost found at ${Boost_INCLUDE_DIRS}")

# Include eos. Todo: We should really rather use an import target approach rather than running add_subdirectory here.
set(eos_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/eos")
set(EOS_BUILD_EXAMPLES OFF)
add_subdirectory(${eos_DIR})

######### The following will find libigl, and build everything that's required for the viewer - e.g. GLFW:
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(LIBIGL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/libigl/include")
find_package(LIBIGL)
if (NOT LIBIGL_FOUND)
   message(FATAL_ERROR "libigl not found --- You can download it using: \n git clone --recursive https://github.com/libigl/libigl.git ${PROJECT_SOURCE_DIR}/../libigl")
endif()
option(LIBIGL_USE_STATIC_LIBRARY "Use LibIGL as static library" OFF)
option(LIBIGL_WITH_NANOGUI     "Use Nanogui menu"   ON) # add a customizable menu bar
# libigl options: choose your dependencies (by default everything is OFF except opengl) 
option(LIBIGL_WITH_VIEWER      "Use OpenGL viewer"  ON)
option(LIBIGL_WITH_OPENGL      "Use OpenGL"         ON)
option(LIBIGL_WITH_OPENGL_GLFW "Use GLFW"           ON)
#option(LIBIGL_WITH_EMBREE      "Use Embree"         OFF)
#option(LIBIGL_WITH_PNG         "Use PNG"            OFF)
# This is going to compile everything you requested 
add_subdirectory("${LIBIGL_INCLUDE_DIR}/../shared/cmake" "libigl")
message("libigl includes: ${LIBIGL_INCLUDE_DIRS}")
message("libigl libraries: ${LIBIGL_LIBRARIES}")
message("libigl extra sources: ${LIBIGL_EXTRA_SOURCES}")
message("libigl extra libraries: ${LIBIGL_EXTRA_LIBRARIES}")
message("libigl definitions: ${LIBIGL_DEFINITIONS}")
######### libigl should be set up by now.

# Set up the eos-model-viewer target:
add_executable(eos-model-viewer eos-model-viewer.cpp)
target_include_directories(eos-model-viewer PRIVATE ${LIBIGL_INCLUDE_DIRS})
add_definitions(${LIBIGL_DEFINITIONS})
target_compile_features(eos-model-viewer PRIVATE ${eos-model-viewer_CXX_COMPILE_FEATURES})
target_link_libraries(eos-model-viewer eos ${OpenCV_LIBS} ${Boost_LIBRARIES} ${LIBIGL_LIBRARIES} ${LIBIGL_EXTRA_LIBRARIES})
target_link_libraries(eos-model-viewer "$<$<CXX_COMPILER_ID:GNU>:-pthread>$<$<CXX_COMPILER_ID:Clang>:-pthreads>")

# Install the binary:
install(TARGETS eos-model-viewer DESTINATION bin)
