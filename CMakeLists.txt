project (xmlrenderer)
set (uvpn_VERSION_MAJOR 1)
set (uvpn_VERSION_MINOR 0)
cmake_minimum_required (VERSION 2.6)


ADD_DEFINITIONS(
    -std=c++11
    -D_GLIBCXX_USE_NANOSLEEP=1
    -Wall
    -Wextra
    -Wno-unused
    -Wno-type-limits
    -DBOOST_ALL_NO_LIB
)

#SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
#SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
SET(CMAKE_C_COMPILER "clang")
SET(CMAKE_CXX_COMPILER "clang++")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

FIND_PACKAGE(libxml++ REQUIRED)
FIND_PACKAGE(Boost 1.53.0 COMPONENTS filesystem system REQUIRED)

INCLUDE_DIRECTORIES(${LibXML++_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
 

add_executable(test1 tests/test.cpp src/xmllib.cpp src/taglib.hpp src/stacked_exception.cpp)
target_link_libraries(test1 ${LibXML++_LIBRARIES} ${Boost_LIBRARIES})
