project (xmlrenderer)
set (uvpn_VERSION_MAJOR 1)
set (uvpn_VERSION_MINOR 0)
cmake_minimum_required (VERSION 2.6)

SET(CXX_FLAGS_BASE "-std=c++11 -D_GLIBCXX_USE_NANOSLEEP=1")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CXX_FLAGS_BASE}")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CXX_FLAGS_BASE} -Wall -Wextra -Wno-unused -Wno-type-limits")
SET(CMAKE_C_COMPILER "clang")
SET(CMAKE_CXX_COMPILER "clang++")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

FIND_PACKAGE(libxml++ REQUIRED)

INCLUDE_DIRECTORIES(${LibXML++_INCLUDE_DIRS})
 

add_executable(test1 tests/test.cpp src/xmllib.cpp src/taglib.hpp src/stacked_exception.cpp)
target_link_libraries(test1 ${LibXML++_LIBRARIES} boost_filesystem boost_system)
