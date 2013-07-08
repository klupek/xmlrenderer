project (xmlrenderer)
set (uvpn_VERSION_MAJOR 1)
set (uvpn_VERSION_MINOR 0)
cmake_minimum_required (VERSION 2.6)

SET(CXX_FLAGS_BASE "-std=c++11 -D_GLIBCXX_USE_NANOSLEEP=1")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CXX_FLAGS_BASE}")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CXX_FLAGS_BASE} -Wall -Wextra -Wno-unused -Wno-type-limits")
SET(CMAKE_C_COMPILER "clang")
SET(CMAKE_CXX_COMPILER "clang++")

INCLUDE_DIRECTORIES(/usr/include/libxml++-2.6 /usr/include/libxml2 /usr/lib64/libxml++-2.6/include /usr/lib64/glibmm-2.4/include /usr/include/sigc++-2.0 /usr/lib64/sigc++-2.0/include /usr/include/glib-2.0 /usr/lib64/glib-2.0/include /usr/include/glibmm-2.4)

add_executable(test1 tests/test.cpp src/xmllib.cpp)
target_link_libraries(test1 xml++-2.6 glibmm-2.4 boost_filesystem boost_system)
