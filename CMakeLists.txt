cmake_minimum_required(VERSION 3.8)

project(NaiveRTC VERSION 0.1.0 LANGUAGES CXX)

# C++ standard requirements
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_TESTS "Enable tests" ON)

set(LIB_SOURCES
    src/pc/candidate.cpp
)

set(LIB_HEADERS
    include/common/defines.hpp
    include/pc/candidate.hpp
)

# naiveRTC
add_library(naivertc STATIC ${LIB_SOURCES} ${LIB_HEADERS} )

# c++17 required
set_target_properties(naivertc PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

# include paths
target_include_directories(naivertc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# liked libraries
# plog
add_subdirectory(deps/plog EXCLUDE_FROM_ALL)
target_link_libraries(naivertc PRIVATE plog::plog)

# test
if(ENABLE_TESTS)

# google test
# EXCLUDE_FROM_ALL表示将指定的目录下的所有targets移出all target列表之外，在执行默认编译时不会被编译。而是在需要时进行收到编译。
# 对于引入的第三方库（一般不会更改其中的代码），建议在需要时使用手动编译以提高编译效率
add_subdirectory(deps/googletest EXCLUDE_FROM_ALL)

set(TEST_SOURCES
    src/pc/candidate_unittest.cpp
    testing/gtest_main.cpp
)

add_executable(naivertc_unittest ${TEST_SOURCES})

set_target_properties(naivertc_unittest PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

target_link_libraries(naivertc_unittest PRIVATE gtest gtest_main gmock gmock_main)
target_link_libraries(naivertc_unittest PRIVATE naivertc)

add_test(NAME unittest COMMAND naivertc_unittest)

endif()

