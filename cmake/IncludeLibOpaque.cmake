cmake_minimum_required(VERSION 3.10)

project(libopaque NONE)

option(GNU_MAKE_PATH "Path to GNU make" "make")

message("Using make=${GNU_MAKE_PATH}")

include(ExternalProject)

# directories for output library and headers
make_directory(${CMAKE_BINARY_DIR}/lib)
make_directory(${CMAKE_BINARY_DIR}/bin)
make_directory(${CMAKE_BINARY_DIR}/include)

ExternalProject_Add(libopaque
GIT_REPOSITORY https://github.com/stef/libopaque.git
GIT_TAG bc83e13732330af044fd27476b9ee0d68217eff6
GIT_SUBMODULES "" # update all submodules
SOURCE_DIR        "${CMAKE_BINARY_DIR}/opaque-src"
BUILD_IN_SOURCE TRUE
CONFIGURE_COMMAND ""
BUILD_COMMAND ${GNU_MAKE_PATH} -C ${CMAKE_BINARY_DIR}/opaque-src/src PREFIX=${CMAKE_BINARY_DIR} CFLAGS="${opaque_CFLAGS}"
INSTALL_COMMAND ${GNU_MAKE_PATH} -C ${CMAKE_BINARY_DIR}/opaque-src/src install PREFIX=${CMAKE_BINARY_DIR}
BUILD_BYPRODUCTS ${opaque_LIBRARY}
)

message("Installed libopaque headers to ${CMAKE_BINARY_DIR}/include, libraries to ${CMAKE_BINARY_DIR}/lib, binaries to ${CMAKE_BINARY_DIR}/bin")
