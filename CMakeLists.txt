cmake_minimum_required(VERSION 3.8)

project(NaiveRTC VERSION 0.1.0 LANGUAGES CXX)

# C++ standard requirements
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_TESTS "Enable tests" ON)

# macos
if(${CMAKE_SYSTEM_NAME} MATCHES "macOS" OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(INSTALL_DIR ${PROJECT_SOURCE_DIR}/lib_deps/macos/_install)
    # openssl
    set(LIBOPENSSL_INCLUDE_PATH "/usr/local/Cellar/openssl\@1.1/1.1.1g/include")
    # boost
    set(LIBBOOST_INCLUDE_PATH ${INSTALL_DIR}/boost/include)
    set(LIBBOOST_LIBRARY_PATH ${INSTALL_DIR}/boost/lib)
else()
    message(FATAL_ERROR "unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

set(LIB_SOURCES
    src/common/utils.cpp
    src/pc/configuration.cpp
    src/pc/candidate.cpp
    src/pc/sdp_serializer.cpp
    src/pc/sdp_entry.cpp
    src/pc/transport.cpp
    src/pc/ice_transport.cpp
    src/signaling/base_channel.cpp
    src/signaling/websocket.cpp
    src/signaling/ayame/ayame_channel.cpp
)

set(LIB_HEADERS
    include/common/defines.hpp
    include/common/utils.hpp
    include/pc/configuration.hpp
    include/pc/candidate.hpp
    include/pc/sdp_serializer.hpp
    include/pc/sdp_entry.hpp
    include/pc/transport.hpp
    include/pc/ice_transport.hpp
    include/signaling/base_channel.hpp
    include/signaling/url_parts.hpp
    include/signaling/websocket.hpp
    include/signaling/ayame/ayame_channel.hpp
)

# naiveRTC
add_library(naivertc STATIC ${LIB_SOURCES} ${LIB_HEADERS} )

# c++17 required
set_target_properties(naivertc PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

# include paths
target_include_directories(naivertc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# liked libraries
# json
add_subdirectory(deps/json)
target_link_libraries(naivertc PRIVATE nlohmann_json)
# plog
add_subdirectory(deps/plog)
target_link_libraries(naivertc PRIVATE plog::plog)
# libnice
add_subdirectory(deps/libjuice)
target_link_libraries(naivertc PRIVATE juice-static)
# sigslot
add_subdirectory(deps/sigslot)
target_link_libraries(naivertc PRIVATE sigslot)
# boost libraries
target_include_directories(naivertc PRIVATE ${LIBBOOST_INCLUDE_PATH})
target_include_directories(naivertc PRIVATE ${LIBOPENSSL_INCLUDE_PATH})
target_link_libraries(naivertc PRIVATE 
    ${LIBBOOST_LIBRARY_PATH}/libboost_filesystem${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${LIBBOOST_LIBRARY_PATH}/libboost_thread${CMAKE_STATIC_LIBRARY_SUFFIX}
)

# test
if(ENABLE_TESTS)

# google test
add_subdirectory(deps/googletest)

set(TEST_SOURCES
    src/pc/candidate_unittest.cpp
    src/pc/configuration_unittest.cpp
    testing/gtest_main.cpp
)

# EXCLUDE_FROM_ALL表示指定的target(或某个模块下的所有target)不会被加入默认编译的all target列表中，即不会自动编译，需要手动编译
add_executable(naivertc_unittest EXCLUDE_FROM_ALL ${TEST_SOURCES})

set_target_properties(naivertc_unittest PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

target_link_libraries(naivertc_unittest PRIVATE gtest gtest_main gmock gmock_main)
target_link_libraries(naivertc_unittest PRIVATE naivertc)

# add_test(NAME unittest COMMAND naivertc_unittest)

endif()

