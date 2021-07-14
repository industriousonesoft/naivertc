cmake_minimum_required(VERSION 3.8)

project(naivertc VERSION 0.1.0 LANGUAGES CXX)

# C++ standard requirements
# 最低版本需求C++11，原因是代码用到了：c++11新标准支持static局部变量安全访问
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# options
# Using libnice instead of libjuice for more completed implementation of 
# the IETF's Interactive Connectivity Establishment(ICE) standard to compatible with WebRTC
option(USE_NICE "Use libnice instead of libjuice" ON)
option(ENABLE_TESTS "Enable tests" ON)

# macOSx
if(${CMAKE_SYSTEM_NAME} MATCHES "macOS" OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(INSTALL_DIR ${PROJECT_SOURCE_DIR}/lib_deps/macos/_install)
    # openssl
    set(OPENSSL_ROOT_DIR "/usr/local/Cellar/openssl\@1.1/1.1.1g/")
    # boost
    set(LIBBOOST_INCLUDE_PATH ${INSTALL_DIR}/boost/include)
    set(LIBBOOST_LIBRARY_PATH ${INSTALL_DIR}/boost/lib)
else()
    message(FATAL_ERROR "unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

set(LIB_SOURCES
    src/base/certificate.cpp
    src/base/tls.cpp
    src/base/packet.cpp
    src/base/init.cpp

    src/common/task_queue.cpp
    src/common/utils.cpp
    src/common/logger.cpp
    src/common/weak_ptr_manager.cpp

    src/signaling/base/base_channel.cpp
    src/signaling/base/websocket.cpp
    src/signaling/ayame/ayame_channel.cpp

    src/pc/sdp/candidate.cpp
    src/pc/sdp/sdp_session_description.cpp
    src/pc/sdp/sdp_entry.cpp
    src/pc/sdp/sdp_defines.cpp
    src/pc/transports/transport.cpp
    src/pc/transports/ice_transport.cpp
    src/pc/transports/ice_transport_juice_delegate.cpp
    src/pc/transports/sctp_transport.cpp
    src/pc/transports/sctp_transport_usrsctp_delegate.cpp
    src/pc/transports/sctp_packet.cpp
    src/pc/transports/dtls_transport.cpp
    src/pc/transports/dtls_transport_openssl_delegate.cpp
    src/pc/transports/dtls_srtp_transport.cpp
    src/pc/transports/dtls_srtp_transport_srtp_delegate.cpp
    src/pc/peer_connection.cpp
    src/pc/peer_connection_configuration.cpp
    src/pc/peer_connection_ice_transport_delegate.cpp
    src/pc/peer_connection_dtls_transport_delegate.cpp
    src/pc/peer_connection_sctp_transport_delegate.cpp
    src/pc/peer_connection_sdp_offer_answer.cpp
    src/pc/peer_connection_media_handler.cpp
    src/pc/channels/channel.cpp
    src/pc/channels/data_channel.cpp
    src/pc/channels/media_channel.cpp
    src/pc/rtp_rtcp/rtp_packet.cpp
    src/pc/rtp_rtcp/rtp_defines.cpp
    src/pc/media/media_track.cpp
    src/pc/media/media_track_sdp_builder.cpp
)

set(LIB_HEADERS
    include/base/defines.hpp
    include/base/certificate.hpp
    include/base/tls.hpp
    include/base/internals.hpp
    include/base/packet.hpp
    include/base/init.hpp

    include/common/task_queue.hpp
    include/common/proxy.hpp
    include/common/utils.hpp
    include/common/logger.hpp
    include/common/weak_ptr_manager.hpp

    include/signaling/base/base_channel.hpp
    include/signaling/common/url_parts.hpp
    include/signaling/base/websocket.hpp
    include/signaling/ayame/ayame_channel.hpp

    include/pc/sdp/candidate.hpp
    include/pc/sdp/sdp_session_description.hpp
    include/pc/sdp/sdp_entry.hpp
    include/pc/sdp/sdp_defines.hpp
    include/pc/transports/transport.hpp
    include/pc/transports/ice_transport.hpp
    include/pc/transports/sctp_transport.hpp
    include/pc/transports/sctp_transport_usr_sctp_settings.hpp
    include/pc/transports/sctp_packet.hpp
    include/pc/transports/dtls_transport.hpp
    include/pc/transports/dtls_srtp_transport.hpp
    include/pc/peer_connection_configuration.hpp
    include/pc/peer_connection.hpp
    include/pc/channels/channel.hpp
    include/pc/channels/data_channel.hpp
    include/pc/channels/media_channel.hpp
    include/pc/rtp_rtcp/rtp_packet.hpp
    include/pc/rtp_rtcp/rtp_defines.hpp
    include/pc/media/media_track.hpp
)

# gcc -Wall: enables all compiler's warning messages
# gcc -Wextra: enables extra warning messages
# gcc -Wpedantic: 当gcc编译不符合ANSI/ISO C标准的代码时，将产生相应的警告信息
set_source_files_properties(${LIB_SOURCES} PROPERTIES COMPILE_FLAGS -Wall -Wextra -Wpedantic)

# naiveRTC
add_library(${PROJECT_NAME} STATIC ${LIB_SOURCES} ${LIB_HEADERS} )

# c++17 required
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

# include paths
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

# linked libraries
# json
add_subdirectory(deps/json EXCLUDE_FROM_ALL)
target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json)
# plog
add_subdirectory(deps/plog EXCLUDE_FROM_ALL)
target_link_libraries(${PROJECT_NAME} PUBLIC plog::plog)

# libnice
if(USE_NICE)
    add_subdirectory(deps/libnice EXCLUDE_FROM_ALL)
    target_link_libraries(${PROJECT_NAME} PUBLIC LibNice::LibNice)
 # libjuice
else()
    add_subdirectory(deps/libjuice EXCLUDE_FROM_ALL)
    target_link_libraries(${PROJECT_NAME} PUBLIC LibJuice::LibJuiceStatic)
endif()

# sigslot
add_subdirectory(deps/sigslot EXCLUDE_FROM_ALL)
target_link_libraries(${PROJECT_NAME} PUBLIC sigslot)
# boost libraries
# PUBLIC: 表示关联naivertc库的库或执行文件可共享naivertc所关联的库
target_include_directories(${PROJECT_NAME} PUBLIC ${LIBBOOST_INCLUDE_PATH})
target_include_directories(${PROJECT_NAME} PUBLIC ${OPENSSL_ROOT_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC 
    ${LIBBOOST_LIBRARY_PATH}/libboost_filesystem${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${LIBBOOST_LIBRARY_PATH}/libboost_thread${CMAKE_STATIC_LIBRARY_SUFFIX}
)
# openssl
if(APPLE)
    # This is a bug in CMake that causes it to prefer the system version over 
    # the one in the specified ROOT folder
    if(EXISTS ${OPENSSL_ROOT_DIR})
        set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.dylib" CACHE FILEPATH "" FORCE)
        set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libssl.dylib" CACHE FILEPATH "" FORCE)
    endif()
endif()
find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC OpenSSL::SSL)
# usrsctp
option(sctp_build_programs 0)
option(sctp_build_shared_lib 0)
add_subdirectory(deps/usrsctp EXCLUDE_FROM_ALL)
add_library(Usrsctp::Usrsctp ALIAS usrsctp)
target_link_libraries(${PROJECT_NAME} PUBLIC Usrsctp::Usrsctp)
# srtp
add_subdirectory(deps/libsrtp EXCLUDE_FROM_ALL)
target_link_libraries(${PROJECT_NAME} PUBLIC srtp2)

# examples
add_subdirectory(${PROJECT_SOURCE_DIR}/examples/streamer ${CMAKE_CURRENT_BINARY_DIR}/examples/streamer EXCLUDE_FROM_ALL)
add_subdirectory(${PROJECT_SOURCE_DIR}/examples/common_examples ${CMAKE_CURRENT_BINARY_DIR}/examples/common_examples EXCLUDE_FROM_ALL)

# test
if(ENABLE_TESTS)

# google test
add_subdirectory(deps/googletest)

set(TEST_SOURCES
    # src/common/task_queue_unittest.cpp
    src/pc/media/media_track_unittest.cpp
    src/pc/sdp/sdp_entry_unittest.cpp
    src/pc/sdp/candidate_unittest.cpp
    src/pc/peer_connection_configuration_unittest.cpp
    src/pc/rtp_rtcp/rtp_defines_unittest.cpp

    testing/gtest_main.cpp
)

# EXCLUDE_FROM_ALL表示指定的target(或某个模块下的所有target)不会被加入默认编译的all target列表中，即不会自动编译，需要手动编译
add_executable(naivertc_unittest EXCLUDE_FROM_ALL ${TEST_SOURCES})

set_target_properties(naivertc_unittest PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

target_link_libraries(naivertc_unittest PRIVATE gtest gtest_main gmock gmock_main)
target_link_libraries(naivertc_unittest PRIVATE ${PROJECT_NAME})

# add_test(NAME unittest COMMAND naivertc_unittest)

endif()

