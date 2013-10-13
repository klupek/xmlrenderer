project (xmlrenderer)
set (uvpn_VERSION_MAJOR 1)
set (uvpn_VERSION_MINOR 0)
cmake_minimum_required (VERSION 2.6)

option(GENERATE_COVERAGE "(broken when using clang)")

set(LibDefinitions "-std=c++11 -Wall -Wextra -Wno-unused -Wno-type-limits -DBOOST_ALL_NO_LIB -DBOOST_SPIRIT_USE_PHOENIX_V3")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

FIND_PACKAGE(libxml++ REQUIRED)
FIND_PACKAGE(libxslt REQUIRED)
FIND_PACKAGE(Boost 1.53.0 COMPONENTS filesystem system unit_test_framework REQUIRED)

add_subdirectory(webpp-common)
link_directories(${xmlrenderer_BINARY_DIR}/webpp-common)

INCLUDE_DIRECTORIES(${xmlrenderer_SOURCE_DIR}/webpp-common ${xmlrenderer_SOURCE_DIR} ${LibXML++_INCLUDE_DIRS} ${LibXSLT_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
 
add_library(xmlrenderer xmlrenderer/xmllib.cpp xmlrenderer/test_parser.cpp xmlrenderer/taglib.hpp)
target_link_libraries(xmlrenderer ${LibXML++_LIBRARIES} ${LibXSLT_LIBRARIES} ${Boost_LIBRARIES} webpp-common)
set_target_properties(xmlrenderer PROPERTIES COMPILE_FLAGS "${LibDefinitions}")

add_executable(renderproc xmlrenderer/renderproc.cpp)
target_link_libraries(renderproc ${LibXML++_LIBRARIES} ${LibXSLT_LIBRARIES} ${Boost_LIBRARIES} webpp-common xmlrenderer)
set_target_properties(renderproc PROPERTIES COMPILE_FLAGS "${LibDefinitions}")

add_executable(parser_test xmlrenderer/parser_test.cpp)
target_link_libraries(parser_test ${LibXML++_LIBRARIES} ${LibXSLT_LIBRARIES} ${Boost_LIBRARIES} webpp-common xmlrenderer)
set_target_properties(parser_test PROPERTIES COMPILE_FLAGS "${LibDefinitions}")

add_dependencies(parser_test xmlrenderer)
add_dependencies(renderproc xmlrenderer)

ENABLE_TESTING()

if(GENERATE_COVERAGE)
		set(CoverageDefinitions "-g -fprofile-arcs -ftest-coverage -DBOOST_TEST_DYN_LINK ${LibDefinitions}")
		set(CoverageLinkDefinitions "-g -fprofile-arcs -ftest-coverage -DBOOST_TEST_DYN_LINK")
else()
		set(CoverageDefinitions "-DBOOST_TEST_DYN_LINK ${LibDefinitions}")
		set(CoverageLinkDefinitions "-DBOOST_TEST_DYN_LINK")
endif()

add_executable(xmlrenderer-test tests/test.cpp)
target_link_libraries(xmlrenderer-test ${LibXML++_LIBRARIES} ${LibXSLT_LIBRARIES} ${Boost_LIBRARIES} webpp-common xmlrenderer)
set_target_properties(xmlrenderer-test PROPERTIES COMPILE_FLAGS ${CoverageDefinitions} LINK_FLAGS ${CoverageLinkDefinitions})

add_test(xmlrenderer-test ${xmlrenderer_BINARY_DIR}/test1)

add_dependencies(xmlrenderer-test xmlrenderer)


