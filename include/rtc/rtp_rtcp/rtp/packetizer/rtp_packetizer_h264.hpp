#ifndef _RTC_MEDIA_VIDEO_H264_RTP_PACKETIZER_H_
#define _RTC_MEDIA_VIDEO_H264_RTP_PACKETIZER_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"
#include "rtc/media/video/codecs/h264/common.hpp"
#include "rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer.hpp"

#include <deque>
#include <memory>
#include <queue>

namespace naivertc {

class RTC_CPP_EXPORT RtpH264Packetizer final: public RtpPacketizer {
public:
    RtpH264Packetizer();
    ~RtpH264Packetizer();

    void Packetize(ArrayView<const uint8_t> payload, 
                   const PayloadSizeLimits& limits, 
                   h264::PacketizationMode packetization_mode = h264::PacketizationMode::NON_INTERLEAVED);

    size_t NumberOfPackets() const override;

    bool NextPacket(RtpPacketToSend* rtp_packet) override;

private:
    // A packet unit (H264 packet), to be put into an RTP packet:
    // If a NAL unit is too large for an RTP packet, this packet unit will
    // represent a FU-A packet of a single fragment of the NAL unit.
    // If a NAL unit is small enough to fit within a single RTP packet, this
    // packet unit may represent a single NAL unit or a STAP-A packet, of which
    // there may be multiple in a single RTP packet (if so, aggregated = true).
    struct PacketUnit {
        PacketUnit(ArrayView<const uint8_t> fragment_data,
                   bool first_fragment,
                   bool last_fragment,
                   bool aggregated,
                   uint8_t header)
            : fragment_data(fragment_data),
              first_fragment(first_fragment),
              last_fragment(last_fragment),
              aggregated(aggregated),
              header(header) {}

        ArrayView<const uint8_t> fragment_data;
        bool first_fragment;
        bool last_fragment;
        bool aggregated;
        uint8_t header;
    };

private:
    bool GeneratePackets(const PayloadSizeLimits& limits, h264::PacketizationMode packetization_mode);

    bool PacketizeSingleNalu(size_t fragment_index, const PayloadSizeLimits& limits);
    bool PacketizeFuA(size_t fragment_index, const PayloadSizeLimits& limits);
    size_t PacketizeStapA(size_t fragment_index, const PayloadSizeLimits& limits);

    void NextSinglePacket(RtpPacketToSend* rtp_packet);
    void NextFuAPacket(RtpPacketToSend* rtp_packet);
    void NextStapAPacket(RtpPacketToSend* rtp_packet);

    void Reset();

private:
    const PayloadSizeLimits limits_;
  
    size_t num_packets_left_;
    std::deque<ArrayView<const uint8_t>> input_fragments_;
    std::queue<PacketUnit> packet_units_;

    DISALLOW_COPY_AND_ASSIGN(RtpH264Packetizer);
};
    
} // namespace naivertc


#endif