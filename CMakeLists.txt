cmake_minimum_required(VERSION 3.8)

project(NaiveRTC VERSION 0.1.0 LANGUAGES CXX)

# C++ standard requirements
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_TESTS "Enable tests" ON)

set(LIB_SOURCES
    src/common/str_utils.cpp
    src/pc/candidate.cpp
    src/pc/sdp_serializer.cpp
    src/pc/sdp_entry.cpp
)

set(LIB_HEADERS
    include/common/defines.hpp
    include/common/str_utils.hpp
    include/pc/candidate.hpp
    include/pc/sdp_serializer.hpp
    include/pc/sdp_entry.hpp
)

# naiveRTC
add_library(naivertc STATIC ${LIB_SOURCES} ${LIB_HEADERS} )

# c++17 required
set_target_properties(naivertc PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

# include paths
target_include_directories(naivertc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# liked libraries
# plog
add_subdirectory(deps/plog)
target_link_libraries(naivertc PRIVATE plog::plog)

# test
if(ENABLE_TESTS)

# google test
add_subdirectory(deps/googletest)

set(TEST_SOURCES
    src/pc/candidate_unittest.cpp
    testing/gtest_main.cpp
)

# EXCLUDE_FROM_ALL表示指定的target(或某个模块下的所有target)不会被加入默认编译的all target列表中，即不会自动编译，需要手动编译
add_executable(naivertc_unittest EXCLUDE_FROM_ALL ${TEST_SOURCES})

set_target_properties(naivertc_unittest PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

target_link_libraries(naivertc_unittest PRIVATE gtest gtest_main gmock gmock_main)
target_link_libraries(naivertc_unittest PRIVATE naivertc)

# add_test(NAME unittest COMMAND naivertc_unittest)

endif()

