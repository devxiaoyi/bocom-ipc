cmake_minimum_required(VERSION 3.10)
project(BOCOM VERSION 0.1 DESCRIPTION "IPC library")

set(BOCOM_DIR "${PROJECT_SOURCE_DIR}/bocom")
set(BOOST_DIR "${PROJECT_SOURCE_DIR}/boost")

# specify the C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include_directories(${BOOST_DIR})
include_directories(${BOCOM_DIR})

add_library(bocom SHARED ${BOCOM_DIR}/bocom_ipc.cpp)
target_link_libraries(bocom -lrt)

add_executable(ipc_pub_test cli/main_pub_queue.c)
target_link_libraries(ipc_pub_test PUBLIC bocom Threads::Threads -lrt)
add_executable(ipc_rec_test cli/main_rec_queue.c)
target_link_libraries(ipc_rec_test PUBLIC bocom Threads::Threads -lrt)

option(ENABLE_UNIT_TESTS "Enable unit tests" ON)
message(STATUS "Enable testing: ${ENABLE_UNIT_TESTS}")

if(ENABLE_UNIT_TESTS)
  include(ExternalProject)
  ExternalProject_Add(googletest
    URL https://github.com/google/googletest/archive/release-1.8.0.zip
    URL_HASH SHA1=667f873ab7a4d246062565fad32fb6d8e203ee73
    INSTALL_COMMAND ""
  )

  ExternalProject_Get_Property(googletest binary_dir source_dir)
  add_library(gtest UNKNOWN IMPORTED)
  add_library(gtest_main UNKNOWN IMPORTED)
  set_target_properties(gtest PROPERTIES
    IMPORTED_LOCATION ${binary_dir}/googlemock/gtest/libgtest.a
  )
  set_target_properties(gtest_main PROPERTIES
    IMPORTED_LOCATION ${binary_dir}/googlemock/gtest/libgtest_main.a
  )
  include_directories(${source_dir}/googletest/include)
  file(GLOB TestSrc test/*.cpp)
  add_executable(unittests ${TestSrc})
  target_link_libraries(unittests PUBLIC bocom gtest gtest_main Threads::Threads -lrt)
  add_test(unittests unittests)
endif()
