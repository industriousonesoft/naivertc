cmake_minimum_required(VERSION 3.8)

project(naivertc VERSION 0.1.0 LANGUAGES CXX)

# C++ standard requirements
# 最低版本需求C++11，原因是代码用到了：c++11新标准支持static局部变量安全访问
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 代码平台无关
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Modules path
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

message(STATUS "SYSTEM_NAME : ${CMAKE_SYSTEM_NAME}" )

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
    # base
    src/base/certificate.cpp
    src/base/tls.cpp
    src/base/init.cpp
    src/base/system_time.cpp
    # common
    src/common/task_queue.cpp
    src/common/utils_string.cpp
    src/common/utils_time.cpp
    src/common/utils_network.cpp
    src/common/logger.cpp
    src/common/weak_ptr_manager.cpp
    # signaling
    src/signaling/base/base_channel.cpp
    src/signaling/base/websocket.cpp
    src/signaling/ayame/ayame_channel.cpp
    # rtc
    # rtc -> base
    src/rtc/base/packet.cpp
    src/rtc/base/ntp_time.cpp
    src/rtc/base/clock.cpp
    src/rtc/base/clock_real_time.cpp
    # rtc -> sdp
    src/rtc/sdp/sdp_utils.cpp
    src/rtc/sdp/candidate.cpp
    src/rtc/sdp/sdp_description.cpp
    src/rtc/sdp/sdp_description_builder.cpp
    src/rtc/sdp/sdp_description_parser.cpp
    src/rtc/sdp/sdp_entry.cpp
    src/rtc/sdp/sdp_media_entry.cpp
    src/rtc/sdp/sdp_session_entry.cpp
    src/rtc/sdp/sdp_media_entry_application.cpp
    src/rtc/sdp/sdp_media_entry_media.cpp
    src/rtc/sdp/sdp_media_entry_audio.cpp
    src/rtc/sdp/sdp_media_entry_video.cpp
    # rtc -> transports
    src/rtc/transports/transport.cpp
    src/rtc/transports/ice_transport.cpp
    src/rtc/transports/ice_transport_description.cpp
    src/rtc/transports/sctp_transport.cpp
    src/rtc/transports/sctp_transport_usrsctp_delegate.cpp
    src/rtc/transports/sctp_message.cpp
    src/rtc/transports/dtls_transport.cpp
    src/rtc/transports/dtls_transport_openssl_delegate.cpp
    src/rtc/transports/dtls_srtp_transport.cpp
    src/rtc/transports/dtls_srtp_transport_srtp_delegate.cpp
    # rtc -> pc
    src/rtc/pc/peer_connection.cpp
    src/rtc/pc/peer_connection_configuration.cpp
    src/rtc/pc/peer_connection_ice_transport_delegate.cpp
    src/rtc/pc/peer_connection_dtls_transport_delegate.cpp
    src/rtc/pc/peer_connection_sctp_transport_delegate.cpp
    src/rtc/pc/peer_connection_sdp_offer_answer.cpp
    src/rtc/pc/peer_connection_media_handler.cpp
    # rtc -> channels
    src/rtc/channels/data_channel.cpp
    src/rtc/channels/data_channel_message.cpp
    src/rtc/channels/media_channel.cpp
    # rtc -> rtp_rtcp
    src/rtc/rtp_rtcp/rtp_packet.cpp
    src/rtc/rtp_rtcp/rtcp_packet.cpp
    src/rtc/rtp_rtcp/rtp_packetization_config.cpp
    src/rtc/rtp_rtcp/rtp_packetizer.cpp
    src/rtc/rtp_rtcp/rtp_packet_to_send.cpp
    src/rtc/rtp_rtcp/rtp_packet_history.cpp
    # rtc -> rtp_rtcp -> rtcp_packets
    src/rtc/rtp_rtcp/rtcp_packets/compound_packet.cpp
    src/rtc/rtp_rtcp/rtcp_packets/common_header.cpp
    src/rtc/rtp_rtcp/rtcp_packets/report_block.cpp
    src/rtc/rtp_rtcp/rtcp_packets/sender_report.cpp
    src/rtc/rtp_rtcp/rtcp_packets/rtp_feedback.cpp
    src/rtc/rtp_rtcp/rtcp_packets/sdes.cpp
    src/rtc/rtp_rtcp/rtcp_packets/receiver_report.cpp
    src/rtc/rtp_rtcp/rtcp_packets/remb.cpp
    src/rtc/rtp_rtcp/rtcp_packets/psfb.cpp
    src/rtc/rtp_rtcp/rtcp_packets/pli.cpp
    src/rtc/rtp_rtcp/rtcp_packets/fir.cpp
    src/rtc/rtp_rtcp/rtcp_packets/nack.cpp
    src/rtc/rtp_rtcp/rtcp_packets/loss_notification.cpp
    # rtc -> media
    src/rtc/media/media_track.cpp
    src/rtc/media/media_track_sdp_builder.cpp
    src/rtc/media/video/h264/nalunit.cpp
    src/rtc/media/video/h264/nalunit_fragment.cpp
)

if(USE_NICE)
    set(LIB_SOURCES ${LIB_SOURCES}
        src/rtc/transports/ice_transport_nice_delegate.cpp
    )
else()
    set(LIB_SOURCES ${LIB_SOURCES}
        src/rtc/transports/ice_transport_juice_delegate.cpp
    )
endif()

set(LIB_HEADERS
    # base
    include/base/defines.hpp
    include/base/certificate.hpp
    include/base/tls.hpp
    include/base/init.hpp
    include/base/system_time.hpp
    # common
    include/common/task_queue.hpp
    include/common/proxy.hpp
    include/common/utils_network.hpp
    include/common/utils_numeric.hpp
    include/common/utils_random.hpp
    include/common/utils_string.hpp
    include/common/utils_time.hpp
    include/common/logger.hpp
    include/common/weak_ptr_manager.hpp
    include/common/numeric_range_checker.hpp
    # signaling
    include/signaling/base/base_channel.hpp
    include/signaling/common/url_parts.hpp
    include/signaling/base/websocket.hpp
    include/signaling/ayame/ayame_channel.hpp
    # rtc
    # rtc -> base
    include/rtc/base/internals.hpp
    include/rtc/base/packet.hpp
    include/rtc/base/byte_io.hpp
    include/rtc/base/byte_io_reader.hpp
    include/rtc/base/byte_io_writer.hpp
    include/rtc/base/ntp_time.hpp
    include/rtc/base/unit_base.hpp
    include/rtc/base/unit_relative.hpp
    include/rtc/base/clock.hpp
    include/rtc/base/clock_real_time.hpp
    include/rtc/base/timestamp.hpp
    include/rtc/base/time_delta.hpp
    include/rtc/base/sequence_number_utils.hpp
    # rtc -> sdp
    include/rtc/sdp/sdp_defines.hpp
    include/rtc/sdp/sdp_utils.hpp
    include/rtc/sdp/candidate.hpp
    include/rtc/sdp/sdp_description.hpp
    include/rtc/sdp/sdp_entry.hpp
    include/rtc/sdp/sdp_media_entry.hpp
    include/rtc/sdp/sdp_session_entry.hpp
    include/rtc/sdp/sdp_media_entry_application.hpp
    include/rtc/sdp/sdp_media_entry_media.hpp
    include/rtc/sdp/sdp_media_entry_audio.hpp
    include/rtc/sdp/sdp_media_entry_video.hpp
    # rtc -> transports
    include/rtc/transports/transport.hpp
    include/rtc/transports/ice_transport.hpp
    include/rtc/transports/sctp_transport.hpp
    include/rtc/transports/sctp_transport_usr_sctp_settings.hpp
    include/rtc/transports/sctp_message.hpp
    include/rtc/transports/dtls_transport.hpp
    include/rtc/transports/dtls_srtp_transport.hpp
    # rtc -> pc
    include/rtc/pc/peer_connection_configuration.hpp
    include/rtc/pc/peer_connection.hpp
    # rtc -> channels
    include/rtc/channels/data_channel.hpp
    include/rtc/channels/media_channel.hpp
    # rtc -> rtp_rtcp
    include/rtc/rtp_rtcp/rtp_rtcp_defines.hpp
    include/rtc/rtp_rtcp/rtp_packet.hpp
    include/rtc/rtp_rtcp/rtcp_packet.hpp
    include/rtc/rtp_rtcp/rtp_packetization_config.hpp
    include/rtc/rtp_rtcp/rtp_packetizer.hpp
    include/rtc/rtp_rtcp/rtp_packet_to_send.hpp
    include/rtc/rtp_rtcp/rtp_packet_history.hpp
    # rtc -> rtp_rtcp -> rtcp_packets
    include/rtc/rtp_rtcp/rtcp_packets/compound_packet.hpp
    include/rtc/rtp_rtcp/rtcp_packets/common_header.hpp
    include/rtc/rtp_rtcp/rtcp_packets/report_block.hpp
    include/rtc/rtp_rtcp/rtcp_packets/sender_report.hpp
    include/rtc/rtp_rtcp/rtcp_packets/rtp_feedback.hpp
    include/rtc/rtp_rtcp/rtcp_packets/sdes.hpp
    include/rtc/rtp_rtcp/rtcp_packets/receiver_report.hpp
    include/rtc/rtp_rtcp/rtcp_packets/remb.hpp
    include/rtc/rtp_rtcp/rtcp_packets/psfb.hpp
    include/rtc/rtp_rtcp/rtcp_packets/pli.hpp
    include/rtc/rtp_rtcp/rtcp_packets/fir.hpp
    include/rtc/rtp_rtcp/rtcp_packets/nack.hpp
    include/rtc/rtp_rtcp/rtcp_packets/loss_notification.hpp
    # rtc -> media
    include/rtc/media/media_track.hpp
    include/rtc/media/video/h264/nalunit.hpp
    include/rtc/media/video/h264/nalunit_fragment.hpp
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

if(USE_NICE)
    # libnice
    find_package(LibNice REQUIRED)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_NICE=1)
    target_link_libraries(${PROJECT_NAME} PUBLIC LibNice::LibNice)
else()
    # libjuice
    add_subdirectory(deps/libjuice EXCLUDE_FROM_ALL)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_NICE=0)
    target_link_libraries(${PROJECT_NAME} PUBLIC LibJuice::LibJuiceStatic)
endif()

# sigslot
# add_subdirectory(deps/sigslot EXCLUDE_FROM_ALL)
# target_link_libraries(${PROJECT_NAME} PUBLIC sigslot)

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
    src/base/system_time_unittest.cpp

    src/common/weak_ptr_manager_unittest.cpp
    # src/common/task_queue_unittest.cpp
    src/rtc/media/media_track_unittest.cpp
    src/rtc/media/video/h264/nalunit_unittest.cpp 
    src/rtc/media/video/h264/nalunit_fragment_unittest.cpp

    src/rtc/base/packet_unittest.cpp
    src/rtc/base/byte_io_unittest.cpp
    src/rtc/base/unit_base_unittest.cpp
    src/rtc/base/clock_unittest.cpp
    src/rtc/base/ntp_time_unittest.cpp
    src/rtc/base/timestamp_unittest.cpp
    src/rtc/base/time_delta_unittest.cpp

    src/rtc/sdp/sdp_media_entry_unittest.cpp
    src/rtc/sdp/candidate_unittest.cpp
    src/rtc/sdp/sdp_description_unittest.cpp

    src/rtc/pc/peer_connection_configuration_unittest.cpp
    
    src/rtc/rtp_rtcp/rtp_packet_unittest.cpp
    src/rtc/rtp_rtcp/rtcp_packets/common_header_unittest.cpp
    src/rtc/rtp_rtcp/rtcp_packets/report_block_unittest.cpp
    src/rtc/rtp_rtcp/rtcp_packets/sender_report_unittest.cpp
    src/rtc/rtp_rtcp/rtcp_packets/sdes_unittest.cpp
    src/rtc/rtp_rtcp/rtcp_packets/receiver_report_unittest.cpp
    src/rtc/rtp_rtcp/rtcp_packets/remb_unittest.cpp
    src/rtc/rtp_rtcp/rtcp_packets/pli_unittest.cpp
    src/rtc/rtp_rtcp/rtcp_packets/fir_unittest.cpp
    src/rtc/rtp_rtcp/rtcp_packets/nack_unittest.cpp
    src/rtc/rtp_rtcp/rtcp_packets/loss_notification_unittest.cpp

    src/rtc/transports/ice_transport_description_unittest.cpp

    testing/gtest_main.cpp
)

# EXCLUDE_FROM_ALL表示指定的target(或某个模块下的所有target)不会被加入默认编译的all target列表中，即不会自动编译，需要手动编译
add_executable(naivertc_unittest EXCLUDE_FROM_ALL ${TEST_SOURCES})

set_target_properties(naivertc_unittest PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

target_link_libraries(naivertc_unittest PRIVATE gtest gtest_main gmock gmock_main)
target_link_libraries(naivertc_unittest PRIVATE ${PROJECT_NAME})

# add_test(NAME unittest COMMAND naivertc_unittest)

endif()

# compile definitions
if(${CMAKE_SYSTEM_NAME} MATCHES "macOS" OR ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_compile_definitions(${PROJECT_NAME} PUBLIC 
        NAIVERTC_POSIX
        NAIVERTC_MAC
    )
else()
    message(FATAL_ERROR "unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

