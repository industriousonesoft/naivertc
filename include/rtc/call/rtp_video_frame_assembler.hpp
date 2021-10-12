#ifndef _RTC_CALL_RTP_VIDEO_FRAME_ASSEMBLER_H_
#define _RTC_CALL_RTP_VIDEO_FRAME_ASSEMBLER_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp_video_header.hpp"
#include "rtc/rtp_rtcp/components/seq_num_utils.hpp"

#include <vector>
#include <memory>
#include <set>

namespace naivertc {

// This class is not thread-safety, the caller MUST provide that.
class RTC_CPP_EXPORT RtpVideoFrameAssembler {
public:
    struct Packet {
        Packet(RtpVideoHeader video_header, 
               uint16_t seq_num, 
               uint8_t payload_type, 
               uint32_t timestamp, 
               bool marker_bit);
        Packet() = default;
        Packet(const Packet&) = delete;
        Packet(Packet&&) = delete;
        Packet& operator=(const Packet&) = delete;
        Packet& operator=(Packet&&) = delete;
        ~Packet() = default;

        RtpVideoHeader video_header;

        // Indicates the packet is continuous with the previous one or not.
        bool continuous = false;
        uint16_t seq_num = 0;
        uint8_t payload_type = 0;
        uint32_t timestamp = 0;
        bool marker_bit = false;
        int time_nacked = -1;
        bool is_first_packet_in_frame = false;
        bool is_last_packet_in_frame = false;
    };
public:
    // `initial_buffer_size` and `max_buffer_size` must always be a power of two
    RtpVideoFrameAssembler(size_t initial_buffer_size, size_t max_buffer_size);
    ~RtpVideoFrameAssembler();

    void Insert(std::unique_ptr<Packet> packet);
    void Clear();

    void Assemble();

private:
    bool ExpandPacketBufferIfNecessary(uint16_t seq_num);
    void ExpandPacketBuffer(size_t new_size);

    void UpdateMissingPackets(uint16_t seq_num, size_t window_size);
    bool IsContinuous(uint16_t seq_num);

private:
    const size_t max_packet_buffer_size_;
    std::vector<std::unique_ptr<Packet>> packet_buffer_;

    uint16_t first_seq_num_;
    bool first_packet_received_;
    bool is_cleared_to_first_seq_num_;
    uint16_t curr_seq_num_;

    std::optional<uint16_t> newest_inserted_seq_num_;
    std::set<uint16_t, seq_num_utils::DescendingComp<uint16_t>> missing_packets_;

};
    
} // namespace naivertc


#endif