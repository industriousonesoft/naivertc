cmake_minimum_required(VERSION 3.8)

project(NaiveRTC VERSION 0.1.0 LANGUAGES CXX)

# C++ standard requirements
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SOURCES
    src/pc/candidate.cpp
)

set(HEADERS
    include/common/defines.hpp
    include/pc/candidate.hpp
)

# plog
add_subdirectory(deps/plog)

# naiveRTC
add_library(naivertc STATIC ${SOURCES} ${HEADERS})

# c++17 required
set_target_properties(naivertc PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

# include paths
target_include_directories(naivertc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# liked libraries
target_link_libraries(naivertc PRIVATE plog::plog)