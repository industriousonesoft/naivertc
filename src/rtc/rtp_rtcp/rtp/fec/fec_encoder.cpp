#include "rtc/rtp_rtcp/rtp/fec/fec_encoder.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_header_writer_ulp.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

#include <plog/Log.h>

namespace naivertc {

std::unique_ptr<FecEncoder> FecEncoder::CreateUlpfecEncoder() {
    auto fec_header_writer = std::make_unique<UlpfecHeaderWriter>();
    return std::unique_ptr<FecEncoder>(new FecEncoder(std::move(fec_header_writer)));
}

FecEncoder::FecEncoder(std::unique_ptr<FecHeaderWriter> fec_header_writer) 
    : fec_header_writer_(std::move(fec_header_writer)),
      generated_fec_packets_(fec_header_writer_->max_fec_packets(), FecPacket(kIpPacketSize)) {}

FecEncoder::~FecEncoder() = default;

bool FecEncoder::Encode(const PacketViewList& media_packets, 
                        uint8_t protection_factor, 
                        size_t num_important_packets, 
                        bool use_unequal_protection, 
                        FecMaskType fec_mask_type) {
    const size_t num_media_packets = media_packets.size();
    if (num_media_packets == 0) {
        return false;
    }
    if (num_important_packets > num_media_packets) {
        return false;
    }

    const size_t max_media_packets = fec_header_writer_->max_media_packets();
    if (num_media_packets > max_media_packets) {
        PLOG_WARNING << "Can not protect " << num_media_packets
                     << " media packets per frame greater than "
                     << max_media_packets << ".";
        return false;
    }
    
    for (const auto& media_packet : media_packets) {
        if (media_packet->size() < kRtpHeaderSize) {
            PLOG_WARNING << "Media packet size " << media_packet->size()
                         << " is smaller than RTP fixed header size.";
            return false;
        }

        // Ensure the FEC packets will fit in a typical MTU
        if (media_packet->size() + fec_header_writer_->max_packet_overhead() + kTransportOverhead > kIpPacketSize) {
            PLOG_WARNING << "Media packet size " << media_packet->size()
                         << " bytes with overhead is larger than "
                         << kIpPacketSize << " bytes.";
        }
    }

    // Prepare generated FEC packets
    size_t num_fec_packets = NumFecPackets(num_media_packets, protection_factor);
    if (num_fec_packets == 0) {
        return true;
    }

    // TODO: Generate FEC packets

    return true;
}

size_t FecEncoder::NumFecPackets(size_t num_media_packets, uint8_t protection_factor) {
    // Result in Q0 with an unsigned round. (四舍五入)
    size_t num_fec_packets = (num_media_packets * protection_factor + (1 << 7)) >> 8;
    // Generate at least one FEC packet if we need protection.
    if (protection_factor > 0 && num_fec_packets == 0) {
        num_fec_packets = 1;
    }
    return num_fec_packets;
}
    
} // namespace naivertc
