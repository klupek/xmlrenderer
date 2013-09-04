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

SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
SET(CMAKE_C_COMPILER "clang")
SET(CMAKE_CXX_COMPILER "clang++")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

FIND_PACKAGE(libxml++ REQUIRED)
FIND_PACKAGE(Boost 1.53.0 COMPONENTS filesystem system REQUIRED)

add_subdirectory(${xmlrenderer_SOURCE_DIR}/webpp-common)
link_directories(${xmlrenderer_BINARY_DIR}/webpp-common)

INCLUDE_DIRECTORIES(${xmlrenderer_SOURCE_DIR}/webpp-common ${LibXML++_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
 
add_library(xmlrenderer xmlrenderer/xmllib.cpp xmlrenderer/taglib.hpp)
target_link_libraries(xmlrenderer ${LibXML++_LIBRARIES} ${Boost_LIBRARIES} webpp-common)

ENABLE_TESTING()
add_executable(test1 tests/test.cpp xmlrenderer/xmllib.cpp xmlrenderer/taglib.hpp)
target_link_libraries(test1 ${LibXML++_LIBRARIES} ${Boost_LIBRARIES} webpp-common)

add_test(xmlrenderer-test ${xmlrenderer_BINARY_DIR}/test1)


