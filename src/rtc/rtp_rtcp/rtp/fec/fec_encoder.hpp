#ifndef _RTC_RTP_RTCP_FEC_ENCODER_H_
#define _RTC_RTP_RTCP_FEC_ENCODER_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_codec.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_header_writer.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_mask_generator.hpp"

#include <list>

namespace naivertc {

// NOTE: This class is not thread safe, the caller must provide that.
class RTC_CPP_EXPORT FecEncoder : public FecCodec {
public:
    // Using static Create method to make sure the FEC coder is unique, 
    // and not allowed to share with others
    static std::unique_ptr<FecEncoder> CreateUlpFecEncoder();

    using PacketList = std::list<std::shared_ptr<RtpPacket>>;
    using FecPacketList = std::vector<CopyOnWriteBuffer>;
public:
    ~FecEncoder() override;

    /** 
     * protection_factor: 
     *      FEC protection overhead in the [0, 255] domain. To obtain 100% overhead, or an
     *      equal number of FEC packets as media packets, use 255.
     * num_important_packets:
     *      The number of "important" packets in the frame. These packets may receive greater 
     *      protection than the remaining packets. The important packets must be located at the 
     *      start of the media packet list. For codecs with data partitioning, the important 
     *      packets may correspond to first partition packets.
     * use_unequal_protection:
     *      Parameter to enable/disable unequal protection (UEP) across packets. Enabling
     *      UEP will allocate more protection to the num_important_packets from the start of the
     *      media_packets
     * fec_mask_type:
     *      The type of packet mask used in the FEC. Random or bursty type may be selected.
     *      The bursty type is only defined up to 12 media packets. If the number of media packets is
     *      above 12, the packet masks from the random table will be selected.
    */
    std::pair<size_t, bool> Encode(const PacketList& media_packets, 
                                   uint8_t protection_factor, 
                                   size_t num_important_packets, 
                                   bool use_unequal_protection, 
                                   FecMaskType fec_mask_type,
                                   FecPacketList& generated_fec_packets);

    // Gets the maximum size of FEC packets will be generated.
    size_t MaxFecPackets() const;

    // Gets the maximum size of the FEC headers in bytes, which must be
    // accounted for as packet overhead.
    size_t MaxPacketOverhead() const;

    // Calculate the actual number of FEC packets will be generated, 
    // given the number of media packets and the protection factor.
    static size_t CalcNumFecPackets(size_t num_media_packets, uint8_t protection_factor);

protected:
    FecEncoder(std::unique_ptr<FecHeaderWriter> fec_header_writer);

private:
    ssize_t InsertZeroInPacketMasks(const PacketList& media_packets, size_t num_fec_packets);

    void GenerateFecPayload(const PacketList& media_packets, 
                            size_t num_fec_packets, 
                            std::vector<CopyOnWriteBuffer>& generated_fec_packets);

    void FinalizeFecHeaders(size_t packet_mask_size, 
                            uint32_t media_ssrc, 
                            uint16_t seq_num_base, 
                            size_t num_fec_packets, 
                            std::vector<CopyOnWriteBuffer>& generated_fec_packets);

private:
    std::unique_ptr<FecHeaderWriter> fec_header_writer_;
    std::unique_ptr<FecPacketMaskGenerator> packet_mask_generator_;
    size_t packet_mask_size_;
    
    static const size_t max_packet_mask_count = kUlpFecMaxMediaPackets * kUlpFecMaxPacketMaskSize;
    uint8_t packet_masks_[max_packet_mask_count];
    uint8_t tmp_packet_masks_[max_packet_mask_count];
};
    
} // namespace naivertc


#endif