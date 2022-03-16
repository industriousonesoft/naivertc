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

# Header files
set(LIB_HEADERS
    # base
    src/base/defines.hpp
    src/base/certificate.hpp
    src/base/tls.hpp
    src/base/init.hpp
    src/base/system_time.hpp
    src/base/thread_annotation.hpp

    # common
    src/common/proxy.hpp
    src/common/utils_network.hpp
    src/common/utils_numeric.hpp
    src/common/utils_random.hpp
    src/common/utils_string.hpp
    src/common/utils_time.hpp
    src/common/logger.hpp
    src/common/weak_ptr_manager.hpp
    src/common/numeric_range_checker.hpp
    src/common/array_view.hpp
    src/common/thread_utils.hpp

    # rtc
    # rtc -> base
    src/rtc/base/internals.hpp
    src/rtc/base/dscp.hpp
    src/rtc/base/packet_options.hpp
    src/rtc/base/copy_on_write_buffer.hpp
    src/rtc/base/arraysize.hpp
    src/rtc/base/memory/byte_io.hpp
    src/rtc/base/memory/byte_io_reader.hpp
    src/rtc/base/memory/byte_io_writer.hpp
    src/rtc/base/memory/bit_io.hpp
    src/rtc/base/memory/bit_io_reader.hpp
    src/rtc/base/memory/bit_io_writer.hpp
    src/rtc/base/units/unit_base.hpp
    src/rtc/base/units/unit_relative.hpp
    src/rtc/base/units/timestamp.hpp
    src/rtc/base/units/time_delta.hpp
    src/rtc/base/units/data_rate.hpp
    src/rtc/base/time/ntp_time.hpp
    src/rtc/base/time/ntp_time_util.hpp
    src/rtc/base/time/clock.hpp
    src/rtc/base/time/clock_real_time.hpp
    src/rtc/base/numerics/moving_median_filter.hpp
    src/rtc/base/numerics/percentile_filter.hpp
    src/rtc/base/numerics/modulo_operator.hpp
    src/rtc/base/numerics/running_statistics.hpp
    src/rtc/base/numerics/histogram_percentile_counter.hpp
    src/rtc/base/numerics/divide_round.hpp
    src/rtc/base/numerics/exp_filter.hpp
    src/rtc/base/task_utils/repeating_task.hpp
    src/rtc/base/task_utils/task_queue.hpp
    src/rtc/base/task_utils/task_queue_impl.hpp
    src/rtc/base/task_utils/task_queue_impl_boost.hpp
    src/rtc/base/synchronization/event.hpp
    src/rtc/base/synchronization/yield_policy.hpp
    src/rtc/base/synchronization/sequence_checker.hpp

    # rtc -> sdp
    src/rtc/sdp/sdp_defines.hpp
    src/rtc/sdp/sdp_utils.hpp
    src/rtc/sdp/candidate.hpp
    src/rtc/sdp/sdp_description.hpp
    src/rtc/sdp/sdp_entry.hpp
    src/rtc/sdp/sdp_media_entry.hpp
    src/rtc/sdp/sdp_session_entry.hpp
    src/rtc/sdp/sdp_media_entry_application.hpp
    src/rtc/sdp/sdp_media_entry_media.hpp

    # rtc -> transports
    src/rtc/transports/base_transport.hpp
    src/rtc/transports/ice_transport.hpp
    src/rtc/transports/sctp_transport.hpp
    src/rtc/transports/sctp_transport_internals.hpp
    src/rtc/transports/sctp_transport_usr_sctp_settings.hpp
    src/rtc/transports/sctp_message.hpp
    src/rtc/transports/dtls_transport.hpp
    src/rtc/transports/dtls_srtp_transport.hpp
    src/rtc/transports/rtc_transport_media.hpp
    src/rtc/transports/rtc_transport_data.hpp

    # rtc -> call
    src/rtc/call/call.hpp
    src/rtc/call/rtp_send_controller.hpp

    # rtc -> pc
    src/rtc/pc/ice_server.hpp
    src/rtc/pc/peer_connection_configuration.hpp
    src/rtc/pc/peer_connection.hpp

    # rtc -> data_channel
    src/rtc/data_channel/data_channel.hpp

    # rtc -> media
    src/rtc/media/media_track.hpp
    src/rtc/media/audio_track.hpp
    src/rtc/media/video_track.hpp
    src/rtc/media/video_receive_stream.hpp
    src/rtc/media/video_send_stream.hpp
    src/rtc/media/media_channel.hpp
    src/rtc/media/media_send_stream.hpp
    src/rtc/media/media_receive_stream.hpp

    # rtc -> media -> video
    src/rtc/media/video/common.hpp
    src/rtc/media/video/encoded_frame.hpp
    src/rtc/media/video/encoded_frame_sink.hpp

    # rtc -> media -> video -> codecs -> h264
    src/rtc/media/video/codecs/h264/common.hpp
    src/rtc/media/video/codecs/h264/nalunit.hpp
    src/rtc/media/video/codecs/h264/nalunit_fragment.hpp
    src/rtc/media/video/codecs/h264/pps_parser.hpp
    src/rtc/media/video/codecs/h264/sps_parser.hpp
    src/rtc/media/video/codecs/h264/sps_pps_tracker.hpp
    src/rtc/media/video/codecs/h264/sprop_parameter_parser.hpp

    # rtc -> rtp_rtcp
    src/rtc/rtp_rtcp/rtcp_responser.hpp
    src/rtc/rtp_rtcp/rtp_sender.hpp
    src/rtc/rtp_rtcp/rtp_sender_video.hpp
    src/rtc/rtp_rtcp/rtp_video_header.hpp
    src/rtc/rtp_rtcp/rtp_video_sender.hpp
    src/rtc/rtp_rtcp/rtp_video_receiver.hpp
    src/rtc/rtp_rtcp/rtx_receive_stream.hpp

    # rtc -> rtp_rtcp -> base
    src/rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp
    src/rtc/rtp_rtcp/base/rtp_rtcp_configurations.hpp
    src/rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp
    src/rtc/rtp_rtcp/base/rtp_extensions.hpp
    src/rtc/rtp_rtcp/base/rtp_parameters.hpp
    src/rtc/rtp_rtcp/base/rtp_statistic_structs.hpp
    src/rtc/rtp_rtcp/base/rtcp_statistic_structs.hpp
    src/rtc/rtp_rtcp/base/rtp_utils.hpp
    src/rtc/rtp_rtcp/base/rtp_packet_sink.hpp

    # rtp -> rtp_rtcp -> components
    src/rtc/rtp_rtcp/components/bit_rate_statistics.hpp
    # src/rtc/rtp_rtcp/components/wrap_around_checker.hpp
    src/rtc/rtp_rtcp/components/rtp_to_ntp_estimator.hpp
    src/rtc/rtp_rtcp/components/remote_ntp_time_estimator.hpp
    src/rtc/rtp_rtcp/components/wrap_around_utils.hpp
    src/rtc/rtp_rtcp/components/num_unwrapper_base.hpp
    src/rtc/rtp_rtcp/components/num_unwrapper.hpp
    src/rtc/rtp_rtcp/components/rolling_accumulator.hpp
    src/rtc/rtp_rtcp/components/rtp_receive_statistics.hpp
    src/rtc/rtp_rtcp/components/rtp_receive_stream_statistician.hpp
    src/rtc/rtp_rtcp/components/rtp_demuxer.hpp

     # rtc -> rtp_rtcp -> rtp -> packet
    src/rtc/rtp_rtcp/rtp/packets/rtp_packet.hpp
    src/rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp
    src/rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp
    src/rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp
    src/rtc/rtp_rtcp/rtp/packets/rtp_header_extension_map.hpp

     # rtc -> rtp_rtcp -> rtp -> sender
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_history.hpp
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.hpp
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.hpp
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp

    # rtc -> rtp_rtcp -> rtp -> receiver
    src/rtc/rtp_rtcp/rtp/receiver/nack_module_impl.hpp
    src/rtc/rtp_rtcp/rtp/receiver/nack_module.hpp

    # rtc -> rtp_rtcp -> rtp -> receiver -> video
    src/rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.hpp
    # rtc -> rtp_rtcp -> rtp -> receiver -> video -> jitter
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_defines.hpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/decoded_frames_history.hpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/packet_buffer.hpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder.hpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder_seq_num.hpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.hpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_estimator.hpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/rtt_filter.hpp

    # rtc -> rtp_rtcp -> rtp -> receiver -> video -> timing
    src/rtc/rtp_rtcp/rtp/receiver/video/timing/decode_time_filter.hpp
    src/rtc/rtp_rtcp/rtp/receiver/video/timing/timestamp_extrapolator.hpp
    src/rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp
    src/rtc/rtp_rtcp/rtp/receiver/video/timing/inter_frame_delay.hpp

    # rtc -> rtp_rtcp -> rtp -> packetizer
    src/rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer.hpp
    src/rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer_h264.hpp

    # rtc -> rtp_rtcp -> rtp -> depacketizer
    src/rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer.hpp
    src/rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer_h264.hpp

    # rtc -> rtp_rtcp -> rtp -> fec
    src/rtc/rtp_rtcp/rtp/fec/fec_defines.hpp
    src/rtc/rtp_rtcp/rtp/fec/fec_codec.hpp
    src/rtc/rtp_rtcp/rtp/fec/fec_encoder.hpp
    src/rtc/rtp_rtcp/rtp/fec/fec_decoder.hpp
    src/rtc/rtp_rtcp/rtp/fec/fec_controller.hpp
    src/rtc/rtp_rtcp/rtp/fec/fec_mask_generator.hpp
    src/rtc/rtp_rtcp/rtp/fec/fec_header_writer.hpp
    src/rtc/rtp_rtcp/rtp/fec/fec_header_reader.hpp
    src/rtc/rtp_rtcp/rtp/fec/fec_generator.hpp
    src/rtc/rtp_rtcp/rtp/fec/fec_test_helper.hpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_header_reader_ulp.hpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_header_writer_ulp.hpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_generator_ulp.hpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_receiver_ulp.hpp
    src/rtc/rtp_rtcp/rtp/fec/flex/fec_generator_flex.hpp

    # rtc -> rtp_rtcp -> rtcp_packets
    src/rtc/rtp_rtcp/rtcp/packets/compound_packet.hpp
    src/rtc/rtp_rtcp/rtcp/packets/common_header.hpp
    src/rtc/rtp_rtcp/rtcp/packets/report_block.hpp
    src/rtc/rtp_rtcp/rtcp/packets/sender_report.hpp
    src/rtc/rtp_rtcp/rtcp/packets/rtpfb.hpp
    src/rtc/rtp_rtcp/rtcp/packets/sdes.hpp
    src/rtc/rtp_rtcp/rtcp/packets/receiver_report.hpp
    src/rtc/rtp_rtcp/rtcp/packets/remb.hpp
    src/rtc/rtp_rtcp/rtcp/packets/psfb.hpp
    src/rtc/rtp_rtcp/rtcp/packets/pli.hpp
    src/rtc/rtp_rtcp/rtcp/packets/fir.hpp
    src/rtc/rtp_rtcp/rtcp/packets/nack.hpp
    src/rtc/rtp_rtcp/rtcp/packets/loss_notification.hpp
    src/rtc/rtp_rtcp/rtcp/packets/tmmb_item.hpp
    src/rtc/rtp_rtcp/rtcp/packets/tmmbr.hpp
    src/rtc/rtp_rtcp/rtcp/packets/tmmbn.hpp
    src/rtc/rtp_rtcp/rtcp/packets/bye.hpp
    src/rtc/rtp_rtcp/rtcp/packets/dlrr.hpp
    src/rtc/rtp_rtcp/rtcp/packets/rrtr.hpp
    src/rtc/rtp_rtcp/rtcp/packets/target_bitrate.hpp
    src/rtc/rtp_rtcp/rtcp/packets/extended_reports.hpp
    src/rtc/rtp_rtcp/rtcp/packets/transport_feedback.hpp

    # rtc -> rtp_rtcp -> rtcp
    src/rtc/rtp_rtcp/rtcp/rtcp_packet.hpp
    src/rtc/rtp_rtcp/rtcp/rtcp_sender.hpp
    src/rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp
    src/rtc/rtp_rtcp/rtcp/rtcp_nack_stats.hpp

    # rtc -> congestion_control -> base
    src/rtc/congestion_control/base/bwe_defines.hpp
    src/rtc/congestion_control/base/network_types.hpp
    src/rtc/congestion_control/base/rate_control_settings.hpp

    # rtc -> congestion_control -> components
    src/rtc/congestion_control/components/interval_budget.hpp
    src/rtc/congestion_control/components/network_transport_statistician.hpp
    src/rtc/congestion_control/components/inter_arrival_delta.hpp
    src/rtc/congestion_control/components/rtt_based_backoff.hpp
    src/rtc/congestion_control/components/alr_detector.hpp
    
    # rtc -> congestion_control -> send_side -> goog_cc -> delay_based
    src/rtc/congestion_control/send_side/goog_cc/delay_based/trendline_estimator.hpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/link_capacity_estimator.hpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/aimd_rate_control.hpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/overuse_detector.hpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/delay_based_bwe.hpp

    # rtc -> congestion_control -> send_side -> goog_cc -> throughput
    src/rtc/congestion_control/send_side/goog_cc/throughput/throughput_estimator.hpp
    src/rtc/congestion_control/send_side/goog_cc/throughput/acknowledged_bitrate_estimator.hpp

    # rtc -> congestion_control -> send_side -> goog_cc -> probe
    src/rtc/congestion_control/send_side/goog_cc/probe/probe_bitrate_estimator.hpp
    src/rtc/congestion_control/send_side/goog_cc/probe/probe_controller.hpp

    # rtc -> congestion_control -> send_side -> goog_cc -> loss_based
    src/rtc/congestion_control/send_side/goog_cc/loss_based/loss_feedback_based_bwe.hpp
    src/rtc/congestion_control/send_side/goog_cc/loss_based/loss_report_based_bwe.hpp

    # rtc -> congestion_control -> send_side -> goog_cc
    src/rtc/congestion_control/send_side/network_controller_interface.hpp
    src/rtc/congestion_control/send_side/goog_cc/send_side_bwe.hpp
    src/rtc/congestion_control/send_side/goog_cc/linker_capacity_tracker.hpp
    src/rtc/congestion_control/send_side/goog_cc/goog_cc_network_controller.hpp
    src/rtc/congestion_control/send_side/goog_cc/congestion_window_pushback_controller.hpp

    # rtc -> congestion_control -> pacing
    src/rtc/congestion_control/pacing/bitrate_prober.hpp
    src/rtc/congestion_control/pacing/round_robin_packet_queue.hpp
    src/rtc/congestion_control/pacing/pacing_controller.hpp
    src/rtc/congestion_control/pacing/task_queue_paced_sender.hpp

)

# Cpp files
set(LIB_SOURCES
    # base
    src/base/certificate.cpp
    src/base/tls.cpp
    src/base/init.cpp
    src/base/system_time.cpp
    # common
    src/common/utils_string.cpp
    src/common/utils_random.cpp
    src/common/utils_time.cpp
    src/common/utils_network.cpp
    src/common/logger.cpp
    src/common/weak_ptr_manager.cpp
    src/common/thread_utils.cpp

    # rtc
    # rtc -> base
    src/rtc/base/packet_options.cpp
    src/rtc/base/copy_on_write_buffer.cpp
    src/rtc/base/memory/bit_io.cpp
    src/rtc/base/memory/bit_io_reader.cpp
    src/rtc/base/memory/bit_io_writer.cpp
    src/rtc/base/time/ntp_time.cpp
    src/rtc/base/time/ntp_time_util.cpp
    src/rtc/base/time/clock.cpp
    src/rtc/base/time/clock_real_time.cpp
    src/rtc/base/numerics/histogram_percentile_counter.cpp
    src/rtc/base/numerics/exp_filter.cpp
    src/rtc/base/task_utils/repeating_task.cpp
    src/rtc/base/task_utils/task_queue.cpp
    src/rtc/base/task_utils/task_queue_impl.cpp
    src/rtc/base/task_utils/task_queue_impl_boost.cpp
    src/rtc/base/synchronization/event.cpp
    # src/rtc/base/synchronization/event_win.cpp
    src/rtc/base/synchronization/event_posix.cpp
    src/rtc/base/synchronization/yield_policy.cpp
    src/rtc/base/synchronization/sequence_checker.cpp
    
    # rtc -> sdp
    src/rtc/sdp/sdp_defines.cpp
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
    src/rtc/sdp/sdp_media_entry_media_auxiliaries.cpp
    src/rtc/sdp/sdp_media_entry_media_builder.cpp
    src/rtc/sdp/sdp_media_entry_media_parser.cpp

    # rtc -> transports
    src/rtc/transports/base_transport.cpp
    src/rtc/transports/ice_transport.cpp
    src/rtc/transports/ice_transport_description.cpp
    src/rtc/transports/sctp_transport.cpp
    src/rtc/transports/sctp_transport_usrsctp_delegate.cpp
    src/rtc/transports/sctp_message.cpp
    src/rtc/transports/dtls_transport.cpp
    src/rtc/transports/dtls_transport_openssl_delegate.cpp
    src/rtc/transports/dtls_srtp_transport.cpp
    src/rtc/transports/dtls_srtp_transport_srtp_delegate.cpp

    # rtc -> call
    src/rtc/call/call.cpp
    src/rtc/call/rtp_send_controller.cpp

    # rtc -> pc
    src/rtc/pc/ice_server.cpp
    src/rtc/pc/peer_connection.cpp
    src/rtc/pc/peer_connection_ice_transport_delegate.cpp
    src/rtc/pc/peer_connection_dtls_transport_delegate.cpp
    src/rtc/pc/peer_connection_sctp_transport_delegate.cpp
    src/rtc/pc/peer_connection_sdp_offer_answer.cpp
    src/rtc/pc/peer_connection_media_handler.cpp

    # rtc -> data_channel
    src/rtc/data_channel/data_channel.cpp
    src/rtc/data_channel/data_channel_utils.cpp
    src/rtc/data_channel/data_channel_message.cpp

    # rtc -> media
    src/rtc/media/media_track.cpp
    src/rtc/media/media_track_configuration.cpp
    src/rtc/media/media_track_sdp.cpp
    src/rtc/media/audio_track.cpp
    src/rtc/media/video_track.cpp
    src/rtc/media/video_receive_stream.cpp
    src/rtc/media/video_send_stream.cpp

    # rtc -> media -> video
    src/rtc/media/video/common.cpp
    src/rtc/media/video/encoded_frame.cpp

    # rtc -> media -> video -> codecs -> h264
    src/rtc/media/video/codecs/h264/nalunit.cpp
    src/rtc/media/video/codecs/h264/nalunit_fragment.cpp
    src/rtc/media/video/codecs/h264/nalunit_auxiliaries.cpp
    src/rtc/media/video/codecs/h264/pps_parser.cpp
    src/rtc/media/video/codecs/h264/sps_parser.cpp
    src/rtc/media/video/codecs/h264/sps_pps_tracker.cpp
    src/rtc/media/video/codecs/h264/sprop_parameter_parser.cpp

    # rtc -> rtp_rtcp
    src/rtc/rtp_rtcp/rtcp_responser.cpp
    src/rtc/rtp_rtcp/rtp_sender.cpp
    src/rtc/rtp_rtcp/rtp_sender_video.cpp
    src/rtc/rtp_rtcp/rtp_video_sender.cpp
    src/rtc/rtp_rtcp/rtp_video_receiver.cpp
    src/rtc/rtp_rtcp/rtp_video_receiver_auxiliaries.cpp
    src/rtc/rtp_rtcp/rtx_receive_stream.cpp

    # rtc -> rtp_rtcp -> base
    src/rtc/rtp_rtcp/base/rtp_utils.cpp
    src/rtc/rtp_rtcp/base/rtp_parameters.cpp
    src/rtc/rtp_rtcp/base/rtp_statistic_structs.cpp
    src/rtc/rtp_rtcp/base/rtcp_statistic_structs.cpp

    # rtc -> rtp_rtcp -> components
    src/rtc/rtp_rtcp/components/bit_rate_statistics.cpp
    src/rtc/rtp_rtcp/components/rtp_to_ntp_estimator.cpp
    src/rtc/rtp_rtcp/components/remote_ntp_time_estimator.cpp
    src/rtc/rtp_rtcp/components/rtp_receive_statistics.cpp
    src/rtc/rtp_rtcp/components/rtp_receive_stream_statistician.cpp
    src/rtc/rtp_rtcp/components/rtp_demuxer.cpp
    src/rtc/rtp_rtcp/components/rtp_demuxer_rtp.cpp
    src/rtc/rtp_rtcp/components/rtp_demuxer_rtcp.cpp

    # rtc -> rtp_rtcp -> rtp -> packet
    src/rtc/rtp_rtcp/rtp/packets/rtp_packet.cpp
    src/rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.cpp
    src/rtc/rtp_rtcp/rtp/packets/rtp_packet_received.cpp
    src/rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.cpp
    src/rtc/rtp_rtcp/rtp/packets/rtp_header_extensions_utils.cpp
    src/rtc/rtp_rtcp/rtp/packets/rtp_header_extension_map.cpp

    # rtc -> rtp_rtcp -> rtp -> sender
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_history.cpp
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.cpp
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_sequencer.cpp
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.cpp
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser_non_paced.cpp

    # rtc -> rtp_rtcp -> rtp -> receiver
    src/rtc/rtp_rtcp/rtp/receiver/nack_module_impl.cpp
    src/rtc/rtp_rtcp/rtp/receiver/nack_module.cpp

    # rtc -> rtp_rtcp -> rtp -> receiver -> video
    src/rtc/rtp_rtcp/rtp/receiver/video/frame_to_decode.cpp

    # rtc -> rtp_rtcp -> rtp -> receiver -> video -> jitter
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/decoded_frames_history.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/packet_buffer.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder_seq_num.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer_continuity.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer_decodability.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_estimator.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/rtt_filter.cpp

    # rtc -> rtp_rtcp -> rtp -> receiver -> video -> timing
    src/rtc/rtp_rtcp/rtp/receiver/video/timing/decode_time_filter.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/timing/timestamp_extrapolator.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/timing/timing.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/timing/inter_frame_delay.cpp

    # rtc -> rtp_rtcp -> rtp -> packetizer
    src/rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer.cpp
    src/rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer_h264.cpp

    # rtc -> rtp_rtcp -> rtp -> depacketizer
    src/rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer_h264.cpp

    # rtc -> rtp_rtcp -> rtp -> fec
    src/rtc/rtp_rtcp/rtp/fec/fec_codec.cpp
    src/rtc/rtp_rtcp/rtp/fec/fec_encoder.cpp
    src/rtc/rtp_rtcp/rtp/fec/fec_decoder.cpp
    src/rtc/rtp_rtcp/rtp/fec/fec_controller.cpp
    src/rtc/rtp_rtcp/rtp/fec/fec_mask_generator.cpp
    src/rtc/rtp_rtcp/rtp/fec/fec_mask_generator_bursty_table.cpp
    src/rtc/rtp_rtcp/rtp/fec/fec_mask_generator_random_table.cpp
    src/rtc/rtp_rtcp/rtp/fec/fec_header_writer.cpp
    src/rtc/rtp_rtcp/rtp/fec/fec_header_reader.cpp
    src/rtc/rtp_rtcp/rtp/fec/fec_test_helper.cpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_header_writer_ulp.cpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_header_reader_ulp.cpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_generator_ulp.cpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_receiver_ulp.cpp
    src/rtc/rtp_rtcp/rtp/fec/flex/fec_generator_flex.cpp

    # rtc -> rtp_rtcp -> rtcp_packets
    src/rtc/rtp_rtcp/rtcp/packets/compound_packet.cpp
    src/rtc/rtp_rtcp/rtcp/packets/common_header.cpp
    src/rtc/rtp_rtcp/rtcp/packets/report_block.cpp
    src/rtc/rtp_rtcp/rtcp/packets/sender_report.cpp
    src/rtc/rtp_rtcp/rtcp/packets/rtpfb.cpp
    src/rtc/rtp_rtcp/rtcp/packets/sdes.cpp
    src/rtc/rtp_rtcp/rtcp/packets/receiver_report.cpp
    src/rtc/rtp_rtcp/rtcp/packets/remb.cpp
    src/rtc/rtp_rtcp/rtcp/packets/psfb.cpp
    src/rtc/rtp_rtcp/rtcp/packets/pli.cpp
    src/rtc/rtp_rtcp/rtcp/packets/fir.cpp
    src/rtc/rtp_rtcp/rtcp/packets/nack.cpp
    src/rtc/rtp_rtcp/rtcp/packets/loss_notification.cpp
    src/rtc/rtp_rtcp/rtcp/packets/tmmb_item.cpp
    src/rtc/rtp_rtcp/rtcp/packets/tmmbr.cpp
    src/rtc/rtp_rtcp/rtcp/packets/tmmbn.cpp
    src/rtc/rtp_rtcp/rtcp/packets/bye.cpp
    src/rtc/rtp_rtcp/rtcp/packets/dlrr.cpp
    src/rtc/rtp_rtcp/rtcp/packets/rrtr.cpp
    src/rtc/rtp_rtcp/rtcp/packets/target_bitrate.cpp
    src/rtc/rtp_rtcp/rtcp/packets/extended_reports.cpp
    src/rtc/rtp_rtcp/rtcp/packets/transport_feedback.cpp
    src/rtc/rtp_rtcp/rtcp/packets/transport_feedback_last_chunk.cpp

    # rtc -> rtp_rtcp -> rtcp
    src/rtc/rtp_rtcp/rtcp/rtcp_packet.cpp
    src/rtc/rtp_rtcp/rtcp/rtcp_sender.cpp
    src/rtc/rtp_rtcp/rtcp/rtcp_sender_builder.cpp
    src/rtc/rtp_rtcp/rtcp/rtcp_sender_auxiliaries.cpp
    src/rtc/rtp_rtcp/rtcp/rtcp_sender_scheduler.cpp
    src/rtc/rtp_rtcp/rtcp/rtcp_receiver.cpp
    src/rtc/rtp_rtcp/rtcp/rtcp_receiver_parser.cpp
    src/rtc/rtp_rtcp/rtcp/rtcp_nack_stats.cpp

    # rtc -> congestion_control -> base
    src/rtc/congestion_control/base/bwe_defines.cpp
    src/rtc/congestion_control/base/network_types.cpp
    src/rtc/congestion_control/base/rate_control_settings.cpp

    # rtc -> congestion_control -> components
    src/rtc/congestion_control/components/interval_budget.cpp
    src/rtc/congestion_control/components/network_transport_statistician.cpp
    src/rtc/congestion_control/components/inter_arrival_delta.cpp
    src/rtc/congestion_control/components/rtt_based_backoff.cpp
    src/rtc/congestion_control/components/alr_detector.cpp

    # rtc -> congestion_control -> send_side -> goog_cc -> delay_based
    src/rtc/congestion_control/send_side/goog_cc/delay_based/overuse_detector.cpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/trendline_estimator.cpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/link_capacity_estimator.cpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/aimd_rate_control.cpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/delay_based_bwe.cpp

    # rtc -> congestion_control -> send_side -> goog_cc -> throughput
    src/rtc/congestion_control/send_side/goog_cc/throughput/throughput_estimator.cpp
    src/rtc/congestion_control/send_side/goog_cc/throughput/acknowledged_bitrate_estimator.cpp

    # rtc -> congestion_control -> send_side -> goog_cc -> probe
    src/rtc/congestion_control/send_side/goog_cc/probe/probe_bitrate_estimator.cpp
    src/rtc/congestion_control/send_side/goog_cc/probe/probe_controller.cpp

    # rtc -> congestion_control -> send_side -> goog_cc -> loss_based
    src/rtc/congestion_control/send_side/goog_cc/loss_based/loss_feedback_based_bwe.cpp
    src/rtc/congestion_control/send_side/goog_cc/loss_based/loss_report_based_bwe.cpp

    # rtc -> congestion_control -> send_side -> goog_cc
    src/rtc/congestion_control/send_side/goog_cc/linker_capacity_tracker.cpp
    src/rtc/congestion_control/send_side/goog_cc/send_side_bwe.cpp
    src/rtc/congestion_control/send_side/goog_cc/goog_cc_network_controller.cpp
    src/rtc/congestion_control/send_side/goog_cc/congestion_window_pushback_controller.cpp

    # rtc -> congestion_control -> pacing
    src/rtc/congestion_control/pacing/bitrate_prober.cpp
    src/rtc/congestion_control/pacing/round_robin_packet_queue.cpp
    src/rtc/congestion_control/pacing/round_robin_packet_queue_augxiliaries.cpp
    src/rtc/congestion_control/pacing/pacing_controller.cpp
    src/rtc/congestion_control/pacing/task_queue_paced_sender.cpp

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

# gcc -Wall: enables all compiler's warning messages
# gcc -Wextra: enables extra warning messages
# gcc -Wpedantic: 当gcc编译不符合ANSI/ISO C标准的代码时，将产生相应的警告信息
set_source_files_properties(${LIB_SOURCES} PROPERTIES COMPILE_FLAGS -Wall -Wextra -Wpedantic)

# Add library
add_library(${PROJECT_NAME} STATIC ${LIB_SOURCES} ${LIB_HEADERS})

# Add precompile headers
# Call after the target has been created by a command such
# as `add_library` or `add_executable`
target_precompile_headers(${PROJECT_NAME} PUBLIC
    "src/base/features_checker.hpp"
    "src/base/attributes_checker.hpp" 
)

# c++17 required
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

# include paths
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/src)

# linked libraries
# json
add_subdirectory(deps/json EXCLUDE_FROM_ALL)
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

# signaling
add_subdirectory(${PROJECT_SOURCE_DIR}/examples/signaling ${CMAKE_CURRENT_BINARY_DIR}/examples/signaling EXCLUDE_FROM_ALL)

# examples
add_subdirectory(${PROJECT_SOURCE_DIR}/examples/streamer ${CMAKE_CURRENT_BINARY_DIR}/examples/streamer EXCLUDE_FROM_ALL)

# test
if(ENABLE_TESTS)

target_compile_definitions(${PROJECT_NAME} PUBLIC ENABLE_TESTS=1)
# google test
add_subdirectory(deps/googletest)

# Header files of unit tests
set(TESTING_SOURCES
    # testing
    src/testing/defines.hpp
    src/testing/simulated_clock.hpp
    src/testing/simulated_clock.cpp
    src/testing/simulated_sequence_runner.hpp
    src/testing/simulated_task_queue.hpp
    src/testing/simulated_task_queue.cpp
    src/testing/simulated_time_controller.hpp
    src/testing/simulated_time_controller.cpp
    src/testing/simulated_time_controller_unittest.cpp

    # rtc -> rtp_rtcp -> rtcp -> 
    src/rtc/rtp_rtcp/rtcp/rtcp_packet_parser.hpp
    src/rtc/rtp_rtcp/rtcp/rtcp_packet_parser.cpp

    # rtc -> congestion_control -> send_side -> goog_cc -> delay_based
    src/rtc/congestion_control/send_side/goog_cc/delay_based/delay_based_bwe_unittest_helper.hpp
    
    # main
    gtest/main.cpp
)

# Cpp files of unit tests
set(UNITTEST_SOURCES
    # base
    src/base/system_time_unittest.cpp
    src/base/thread_annotation_unittest.cpp
    # common
    src/common/weak_ptr_manager_unittest.cpp

    # rtc -> media
    src/rtc/media/media_track_unittest.cpp
    src/rtc/media/video/codecs/h264/nalunit_unittest.cpp 
    src/rtc/media/video/codecs/h264/nalunit_fragment_unittest.cpp
    src/rtc/media/video/codecs/h264/pps_parser_unittest.cpp
    src/rtc/media/video/codecs/h264/sps_parser_unittest.cpp
    src/rtc/media/video/codecs/h264/sps_pps_tracker_unittest.cpp
    src/rtc/media/video/codecs/h264/sprop_parameter_parser_unittest.cpp

    # rtc -> base
    src/rtc/base/memory/byte_io_unittest.cpp
    src/rtc/base/memory/bit_io_reader_unittest.cpp
    src/rtc/base/memory/bit_io_writer_unittest.cpp
    src/rtc/base/time/clock_unittest.cpp
    src/rtc/base/time/ntp_time_unittest.cpp
    src/rtc/base/copy_on_write_buffer_unittest.cpp
    src/rtc/base/units/unit_base_unittest.cpp
    src/rtc/base/units/timestamp_unittest.cpp
    src/rtc/base/units/time_delta_unittest.cpp
    src/rtc/base/units/data_rate_unittest.cpp
    src/rtc/base/numerics/percentile_filter_unittest.cpp
    src/rtc/base/numerics/moving_median_filter_unittest.cpp
    src/rtc/base/numerics/modulo_operator_unittest.cpp
    src/rtc/base/numerics/running_statistics_unittest.cpp
    src/rtc/base/numerics/histogram_percentile_counter_unittest.cpp
    src/rtc/base/numerics/divide_round_unittest.cpp
    src/rtc/base/numerics/exp_filter_unittest.cpp
    src/rtc/base/synchronization/event_unittest.cpp
    src/rtc/base/synchronization/yield_policy_unittest.cpp
    src/rtc/base/task_utils/task_queue_unittest.cpp
    src/rtc/base/task_utils/repeating_task_unittest.cpp

    # rtc -> sdp
    src/rtc/sdp/sdp_media_entry_unittest.cpp
    src/rtc/sdp/candidate_unittest.cpp
    src/rtc/sdp/sdp_description_unittest.cpp

    # rtc -> pc
    src/rtc/pc/ice_server_unittest.cpp

    # rtc -> rtp_rtcp
    src/rtc/rtp_rtcp/rtp_sender_unittest.cpp
    src/rtc/rtp_rtcp/rtp_sender_video_unittest.cpp

    # rtc -> rtp_rtcp -> components
    src/rtc/rtp_rtcp/components/bit_rate_statistics_unittest.cpp
    src/rtc/rtp_rtcp/components/rtp_to_ntp_estimator_unittest.cpp
    src/rtc/rtp_rtcp/components/remote_ntp_time_estimator_unittest.cpp
    src/rtc/rtp_rtcp/components/wrap_around_utils_unittest.cpp
    src/rtc/rtp_rtcp/components/num_unwrapper_base_unittest.cpp
    # src/rtc/rtp_rtcp/components/wrap_around_checker_unittest.cpp
    src/rtc/rtp_rtcp/components/num_unwrapper_unittest.cpp
    src/rtc/rtp_rtcp/components/rolling_accumulator_unittest.cpp
    src/rtc/rtp_rtcp/components/rtp_receive_statistics_unittest.cpp

    # rtc -> rtp_rtcp -> rtp -> packets
    src/rtc/rtp_rtcp/rtp/packets/rtp_packet_unittest.cpp
    src/rtc/rtp_rtcp/rtp/packets/rtp_header_extension_manager_unittest.cpp
    
    # rtc -> rtp_rtcp -> rtp -> packetizer
    src/rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer_h264_unittest.cpp

    # rtc -> rtp_rtcp -> rtp -> depacketizer
    src/rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer_h264_unittest.cpp

    # rtc -> rtp_rtcp -> rtp -> fec
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_header_writer_ulp_unittest.cpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_header_reader_ulp_unittest.cpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_generator_ulp_unittest.cpp
    src/rtc/rtp_rtcp/rtp/fec/ulp/fec_receiver_ulp_unittest.cpp

    # rtc -> rtp_rtcp -> rtp -> receiver
    src/rtc/rtp_rtcp/rtp/receiver/nack_module_impl_unittest.cpp

    # rtc -> rtp_rtcp -> rtp -> receiver -> video -> timing
    src/rtc/rtp_rtcp/rtp/receiver/video/timing/timing_unittest.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/timing/timestamp_extrapolator_unittest.cpp

    # rtc -> rtp_rtcp -> rtp -> receiver -> video -> jitter
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/decoded_frames_history_unittest.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/packet_buffer_unittest.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder_unittest.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer_unittest.cpp
    src/rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_estimator_unittest.cpp

    # rtc -> rtp_rtcp -> rtp -> sender
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser_unittest.cpp
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_history_unittest.cpp
    src/rtc/rtp_rtcp/rtp/sender/rtp_packet_generator_unittest.cpp

    # rtc -> rtp_rtcp -> rtcp 
    src/rtc/rtp_rtcp/rtcp/rtcp_nack_stats_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/common_header_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/compound_packet_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/report_block_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/sender_report_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/sdes_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/receiver_report_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/remb_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/pli_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/fir_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/nack_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/loss_notification_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/tmmbr_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/tmmbn_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/bye_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/dlrr_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/rrtr_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/target_bitrate_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/extended_reports_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/packets/transport_feedback_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/rtcp_receiver_unittest.cpp
    src/rtc/rtp_rtcp/rtcp/rtcp_sender_unittest.cpp

    # rtc -> transports
    src/rtc/transports/ice_transport_description_unittest.cpp

    # rtc -> congestion_control -> components
    src/rtc/congestion_control/components/interval_budget_unittest.cpp
    src/rtc/congestion_control/components/alr_detector_unittest.cpp

    # rtc -> congestion_control -> send_side -> goog_cc -> delay_based
    src/rtc/congestion_control/send_side/goog_cc/delay_based/trendline_estimator_unittest.cpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/aimd_rate_control_unittest.cpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/delay_based_bwe_unittest_helper.cpp
    src/rtc/congestion_control/send_side/goog_cc/delay_based/delay_based_bwe_unittest.cpp

    # rtc -> congestion_control -> send_side -> goog_cc -> throughput
    src/rtc/congestion_control/send_side/goog_cc/throughput/acknowledged_bitrate_estimator_unittest.cpp

    # rtc -> congestion_control -> send_side -> goog_cc -> probe
    src/rtc/congestion_control/send_side/goog_cc/probe/probe_bitrate_estimator_unittest.cpp
    src/rtc/congestion_control/send_side/goog_cc/probe/probe_controller_unittest.cpp

    # rtc -> congestion_control -> send_side -> goog_cc
    src/rtc/congestion_control/send_side/goog_cc/send_side_bwe_unittest.cpp
    src/rtc/congestion_control/send_side/goog_cc/congestion_window_pushback_controller_unittest.cpp

    # rtc -> congestion_control -> pacing
    src/rtc/congestion_control/pacing/bitrate_prober_unittest.cpp
    src/rtc/congestion_control/pacing/pacing_controller_unittest.cpp
    src/rtc/congestion_control/pacing/task_queue_paced_sender_unittest.cpp
    
)

# EXCLUDE_FROM_ALL表示指定的target(或某个模块下的所有target)不会被加入默认编译的all target列表中，即不会自动编译，需要手动编译
add_executable(naivertc_unittest EXCLUDE_FROM_ALL ${TESTING_SOURCES} ${UNITTEST_SOURCES})

set_target_properties(naivertc_unittest PROPERTIES VERSION ${PROJECT_VERSION} CXX_STANDARD 17)

target_link_libraries(naivertc_unittest PRIVATE gtest gtest_main gmock gmock_main)
target_link_libraries(naivertc_unittest PRIVATE ${PROJECT_NAME})

# add_test(NAME unittest COMMAND naivertc_unittest)

else()

target_compile_definitions(${PROJECT_NAME} PUBLIC ENABLE_TESTS=0)

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

