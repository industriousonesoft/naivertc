#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_PACKET_BUFFER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_PACKET_BUFFER_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp_video_header.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"
#include "rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer.hpp"

#include <vector>
#include <memory>
#include <set>

namespace naivertc {
namespace rtp {
namespace video {

using FrameType = ::naivertc::video::FrameType;
using CodecType = ::naivertc::video::CodecType;

namespace jitter {
    
// This class is not thread-safety, the caller MUST provide that.
class PacketBuffer {
public:
    struct Packet {
        Packet(RtpVideoHeader video_header,
               RtpVideoCodecHeader video_codec_header,
               uint16_t seq_num,
               uint32_t timestamp,
               int64_t received_time_ms);
        Packet() = default;
        Packet(const Packet&) = delete;
        Packet(Packet&&) = delete;
        Packet& operator=(const Packet&) = delete;
        Packet& operator=(Packet&&) = delete;
        ~Packet() = default;

        int width() const { return video_header.frame_width; }
        int height() const { return video_header.frame_height; }

        bool is_first_packet_in_frame() const {
            return video_header.is_first_packet_in_frame;
        }
        bool is_last_packet_in_frame() const {
            return video_header.is_last_packet_in_frame;
        }

        RtpVideoHeader video_header;
        RtpVideoCodecHeader video_codec_header;
        CopyOnWriteBuffer video_payload;

        // Indicates the packet is continuous with the previous one.
        bool continuous = false;
        // Packet info
        uint16_t seq_num = 0;
        uint32_t timestamp = 0;
        int64_t received_time_ms = -1;
        int times_nacked = -1;
    };

    struct Frame {
        int frame_width;
        int frame_height;
        video::FrameType frame_type;
        video::CodecType codec_type;
        uint16_t seq_num_start = 0;
        uint16_t seq_num_end = 0;
        uint32_t timestamp = 0;
        int times_nacked = -1;
        int64_t min_received_time_ms = -1;
        int64_t max_received_time_ms = -1;

        size_t num_packets = 0;
        CopyOnWriteBuffer bitstream;
    };
    
    using AssembledFrames = std::vector<std::unique_ptr<Frame>>;
    struct InsertResult {
        AssembledFrames assembled_frames;
        // Indicates if the packet buffer was cleared, which means
        // that a key frame request should be sent.
        bool keyframe_requested = false;
    };
public:
    // `initial_buffer_size` and `max_buffer_size` must always be a power of two
    PacketBuffer(size_t initial_buffer_size, size_t max_buffer_size);
    ~PacketBuffer();

    bool sps_pps_idr_is_h264_keyframe() const { return sps_pps_idr_is_h264_keyframe_; }
    void set_sps_pps_idr_is_h264_keyframe(bool flag) { sps_pps_idr_is_h264_keyframe_ = flag; }

    InsertResult InsertPacket(std::unique_ptr<Packet> packet);
    InsertResult InsertPadding(uint16_t seq_num);
    void Clear();
    void ClearTo(uint16_t seq_num);

private:
    bool ExpandPacketBufferIfNecessary(uint16_t seq_num);
    void ExpandPacketBuffer(size_t new_size);

    void UpdateMissingPackets(uint16_t seq_num, size_t window_size);
    bool IsContinuous(uint16_t seq_num);

    AssembledFrames TryToAssembleFrames(uint16_t seq_num);

private:
    const size_t max_packet_buffer_size_;
    std::vector<std::unique_ptr<Packet>> packet_buffer_;

    uint16_t first_seq_num_;
    bool first_packet_received_;
    bool is_cleared_to_first_seq_num_;
    bool sps_pps_idr_is_h264_keyframe_;

    std::optional<uint16_t> newest_inserted_seq_num_;
    std::set<uint16_t, wrap_around_utils::OlderThan<uint16_t>> missing_packets_;

};
    
} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc


#endif