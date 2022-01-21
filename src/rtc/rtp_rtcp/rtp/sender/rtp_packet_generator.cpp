#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
template <typename Extension>
constexpr rtp::ExtensionSize CreateExtensionSize() {
  return {Extension::kType, Extension::kValueSizeBytes};
}

template <typename Extension>
constexpr rtp::ExtensionSize CreateMaxExtensionSize() {
  return {Extension::kType, Extension::kMaxValueSizeBytes};
}

// Size info for header extensions that might be used in padding or FEC packets.
constexpr rtp::ExtensionSize kFecOrPaddingExtensionSizes[] = {
    CreateExtensionSize<rtp::AbsoluteSendTime>(),
    CreateExtensionSize<rtp::TransmissionTimeOffset>(),
    CreateExtensionSize<rtp::TransportSequenceNumber>(),
    CreateExtensionSize<rtp::PlayoutDelayLimits>(),
    CreateMaxExtensionSize<rtp::RtpMid>(),
    // CreateExtensionSize<VideoTimingExtension>(),
};


} // namespace 

RtpPacketGenerator::RtpPacketGenerator(const RtpConfiguration& config) 
    : ssrc_(config.local_media_ssrc),
      rtx_ssrc_(config.rtx_send_ssrc),
      max_packet_size_(kIpPacketSize - kTransportOverhead), // Default is UDP/IPv4.
      extension_manager_(config.extmap_allow_mixed) {
    UpdateHeaderSizes();
}

RtpPacketGenerator::~RtpPacketGenerator() {}

uint32_t RtpPacketGenerator::ssrc() const {
    RTC_RUN_ON(&sequence_checker_);
    return ssrc_;
}

size_t RtpPacketGenerator::max_rtp_packet_size() const {
    RTC_RUN_ON(&sequence_checker_);
    return max_packet_size_;
}

void RtpPacketGenerator::set_max_rtp_packet_size(size_t max_size) {
    RTC_RUN_ON(&sequence_checker_);
    assert(max_size >= 100);
    assert(max_size <= kIpPacketSize);
    max_packet_size_ = max_size;
}

RtpPacketToSend RtpPacketGenerator::AllocatePacket() const {
    RTC_RUN_ON(&sequence_checker_);
    // While sending slightly oversized packet increase chance of dropped packet,
    // it is better than crash on drop packet without trying to send it.
    // TODO: Find better motivator and value for extra capacity.
    static constexpr int kExtraCapacity = 16;
    auto packet = RtpPacketToSend(&extension_manager_, max_packet_size_ + kExtraCapacity);
    packet.set_ssrc(ssrc_);
    packet.set_csrcs(csrcs_);
    // TODO: Reserve extensions if registered
    return packet;
}

size_t RtpPacketGenerator::FecOrPaddingPacketMaxRtpHeaderSize() const {
    RTC_RUN_ON(&sequence_checker_);
    return max_padding_fec_packet_header_;
}

// RTX
std::optional<uint32_t> RtpPacketGenerator::rtx_ssrc() const {
    RTC_RUN_ON(&sequence_checker_);
    return rtx_ssrc_;
}

void RtpPacketGenerator::SetRtxPayloadType(int payload_type, int associated_payload_type) {
    RTC_RUN_ON(&sequence_checker_);
    // All RTP packet type MUST be less than 127 to distingush with RTCP packet, which MUST be greater than 128.
    // RFC 5761 Multiplexing RTP and RTCP 4. Distinguishable RTP and RTCP Packets
    // https://tools.ietf.org/html/rfc5761#section-4
    assert(payload_type <= 127);
    assert(associated_payload_type <= 127);
    if (payload_type < 0) {
        PLOG_WARNING << "Invalid RTX payload type: " << payload_type;
        return;
    }

    rtx_payload_type_map_[associated_payload_type] = payload_type;
}

std::optional<RtpPacketToSend> RtpPacketGenerator::BuildRtxPacket(const RtpPacketToSend& packet) {
    RTC_RUN_ON(&sequence_checker_);
    if (!rtx_ssrc_.has_value()) {
        PLOG_WARNING << "Failed to build RTX packet withou RTX ssrc.";
        return std::nullopt;
    }
    auto kv = rtx_payload_type_map_.find(packet.payload_type());
    if (kv == rtx_payload_type_map_.end()) {
        return std::nullopt;
    }

    auto rtx_packet = RtpPacketToSend(max_packet_size_);
    rtx_packet.set_payload_type(kv->second);
    rtx_packet.set_ssrc(rtx_ssrc_.value());

    CopyHeaderAndExtensionsToRtxPacket(packet, &rtx_packet);

    uint8_t* rtx_payload = rtx_packet.AllocatePayload(packet.payload_size() + kRtxHeaderSize);

    // Add OSN (original sequence number)
    ByteWriter<uint16_t>::WriteBigEndian(rtx_payload, packet.sequence_number());

    // Copy original payload data
    memcpy(rtx_payload + kRtxHeaderSize, packet.payload().data(), packet.payload_size());

    // TODO: To add original addtional data if necessary

    // FIXME: Copy capture time so e.g. TransmissionOffset is correctly set. Why?
    rtx_packet.set_capture_time_ms(packet.capture_time_ms());
    
    return rtx_packet;
}

// Private methods
void RtpPacketGenerator::UpdateHeaderSizes() {
    const size_t rtp_header_size_without_extenions = kRtpHeaderSize + sizeof(uint32_t) * csrcs_.size();
    max_padding_fec_packet_header_ = rtp_header_size_without_extenions + 
                                     rtp::CalculateRegisteredExtensionSize(kFecOrPaddingExtensionSizes, extension_manager_);
    
    // TODO: To calculate the maximum size of media packet header
}

void RtpPacketGenerator::CopyHeaderAndExtensionsToRtxPacket(const RtpPacketToSend& packet, RtpPacketToSend* rtx_packet) {
    // Set the relevant fixed fields in the packet headers.
    // The following are not set:
    // - Payload type: it is replaced in RTX packet.
    // - Sequence number: RTX has a seperate sequence numbering.
    // - SSRC: RTX stream has its own SSRC
    rtx_packet->set_marker(packet.marker());
    rtx_packet->set_timestamp(packet.timestamp());

    // Set the variable fields in the packet header
    // - CSRCs: must be set before header extensions
    // - Header extensions: repalce Rid header with RepairedRid header
    const std::vector<uint32_t> csrcs = packet.csrcs();
    rtx_packet->set_csrcs(csrcs);

    for (int index = int(RtpExtensionType::NONE) + 1; index < int(RtpExtensionType::NUMBER_OF_EXTENSIONS); ++index) {
        auto extension_type = static_cast<RtpExtensionType>(index);

        // Stream ID header extension (MID, RID) are sent per-SSRC. Since RTX
        // operates on a different SSRC, the presence and values of these header
        // extensions should be determined separately and not blindly copied.
        if (extension_type == RtpExtensionType::MID || 
            extension_type == RtpExtensionType::RTP_STREAM_ID) {
            continue;
        }

        if (!packet.HasExtension(extension_type)) {
            continue;
        }

        ArrayView<const uint8_t> source = packet.FindExtension(extension_type);
        ArrayView<uint8_t> destination = rtx_packet->AllocateExtension(extension_type, source.size());
        
        // Could happen if any:
        // 1. Extension has 0 length.
        // 2. Extension is not registered in destination.
        // 3. Allocating extension in destination failed.
        if (destination.empty() || source.size() != destination.size()) {
            continue;
        }

        std::memcpy(destination.begin(), source.begin(), destination.size());
    }
}
 
} // namespace naivertc
