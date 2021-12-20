#include "rtc/rtp_rtcp/rtp/depacketizer/rtp_depacketizer_h264.hpp"
#include "rtc/media/video/codecs/h264/common.hpp"
#include "rtc/media/video/codecs/h264/pps_parser.hpp"
#include "rtc/media/video/codecs/h264/sps_parser.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
constexpr size_t kNalHeaderSize = 1;
constexpr size_t kFuAHeaderSize = 2;
constexpr size_t kLengthFieldSize = 2;
constexpr size_t kStapAHeaderSize = kNalHeaderSize + kLengthFieldSize;

// Bit masks for FU (A and B) indicators.
enum NalDefs : uint8_t { kFBit = 0x80, kNriMask = 0x60, kTypeMask = 0x1F };
// Bit masks for FU (A and B) headers.
enum FuDefs : uint8_t { kSBit = 0x80, kEBit = 0x40, kRBit = 0x20 };

// Aggregate fragments into one packet (STAP-A)
// e.g.: An RTP packet including an STAP-A containing two 
// single-time aggregation units
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                          RTP Header                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |STAP-A NAL HDR |         NALU 1 Size           | NALU 1 HDR    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         NALU 1 Data                           |
// :                                                               :
// +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |               | NALU 2 Size                   | NALU 2 HDR    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         NALU 2 Data                           |
// :                                                               :
// |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                               :...OPTIONAL RTP padding        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
std::optional<std::vector<h264::NaluIndex>> ParseNaluIndicesPerStapAPacket(const uint8_t* nalu_buffer, size_t buffer_size) {
    std::vector<h264::NaluIndex> nalu_indices;
    size_t offset = kNalHeaderSize;
    while (offset < buffer_size) {
        if (offset + kLengthFieldSize > buffer_size) {
            return std::nullopt;
        }
        uint16_t nalu_payload_size = ByteReader<uint16_t>::ReadBigEndian(nalu_buffer + offset);
        h264::NaluIndex nalu_index;
        nalu_index.start_offset = offset;
        nalu_index.payload_start_offset = offset + kLengthFieldSize;
        if (nalu_index.payload_start_offset + nalu_payload_size > buffer_size) {
            // Return null as this is a truncated STAP-A packet.
            return std::nullopt;
        }
        nalu_index.payload_size = nalu_payload_size;
        nalu_indices.push_back(std::move(nalu_index));
        offset = nalu_index.payload_start_offset + nalu_index.payload_size;
    }
    return nalu_indices;
}

} // namespace

std::optional<RtpDepacketizer::Packet> RtpH264Depacketizer::Depacketize(CopyOnWriteBuffer rtp_payload) {
    if (rtp_payload.size() == 0) {
        PLOG_WARNING << "Failed to depacketize empty paylaod.";
        return std::nullopt;
    }
    uint8_t nal_type = rtp_payload.cdata()[0] & kTypeMask;
    if (nal_type == uint8_t(h264::NaluType::FU_A)) {
        // Fragmented NAL units (FU-A)
        return DepacketizeFuANalu(std::move(rtp_payload));
    } else {
        // We handle STAP-A and single NALU in same way here.
        // The jitter buffer will depacketize the STAP-A into NAL units later.
        return DepacketizeStapAOrSingleNalu(std::move(rtp_payload));
    }
}

// Private methods
std::optional<RtpDepacketizer::Packet> RtpH264Depacketizer::DepacketizeStapAOrSingleNalu(CopyOnWriteBuffer rtp_payload) {
    const uint8_t* const payload_data = rtp_payload.cdata();
    size_t payload_size = rtp_payload.size();
    Packet depacketized_payload;
    depacketized_payload.video_payload = rtp_payload;
    depacketized_payload.video_header.frame_width = 0;
    depacketized_payload.video_header.frame_height = 0;
    depacketized_payload.video_header.codec_type = video::CodecType::H264;
    depacketized_payload.video_header.frame_type = video::FrameType::DELTA;
    depacketized_payload.video_header.is_first_packet_in_frame = true;
    auto& h264_video_codec_header = depacketized_payload.video_codec_header.emplace<h264::PacketizationInfo>();

    auto nalu_parser = [&](size_t nalu_start_offset, size_t nalu_size) mutable -> bool {
        if (nalu_size < kNalHeaderSize) {
            PLOG_WARNING << "STAP-A packet too short.";
            return false;
        }
        
        h264::NaluInfo nalu_info;
        nalu_info.type = payload_data[nalu_start_offset] & kTypeMask;
        nalu_info.sps_id = -1;
        nalu_info.pps_id = -1;
        nalu_info.offset = nalu_start_offset;
        nalu_info.size = nalu_size;
        const uint8_t* const nalu_payload = &payload_data[nalu_start_offset + kNalHeaderSize];
        size_t nalu_payload_size = nalu_size - kNalHeaderSize;

        switch (nalu_info.type) {
        case h264::NaluType::SPS: {
            std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(nalu_payload, nalu_payload_size);
            if (sps.has_value()) {
                // TODO: Parse VUI parameters if necessary.
                if (sps->vui_params_present != 0) {
                    PLOG_WARNING << "Has VUI parameters in SPS slice to parse.";
                }
                depacketized_payload.video_header.frame_width = sps->width;
                depacketized_payload.video_header.frame_height = sps->height;
                nalu_info.sps_id = sps->id;
            } else {
                PLOG_WARNING << "Failed to parse SPS id from SPS slice.";
            }
            depacketized_payload.video_header.frame_type = video::FrameType::KEY;
            h264_video_codec_header.has_sps = true;
            break;
        }
        case h264::NaluType::PPS: {
            uint32_t pps_id;
            uint32_t sps_id;
            if (PpsParser::ParsePpsIds(nalu_payload, nalu_payload_size, &pps_id, &sps_id)) {
                nalu_info.pps_id = pps_id;
                nalu_info.sps_id = sps_id;
            } else {
                PLOG_WARNING << "Failed to parse PPS id and SPS id from PPS slice.";
            }
            h264_video_codec_header.has_pps = true;
            break;
        }
        case h264::NaluType::IDR:
            depacketized_payload.video_header.frame_type = video::FrameType::KEY;
            h264_video_codec_header.has_idr = true;
            // fallthrough
        case h264::NaluType::SLICE: {
            std::optional<uint32_t> pps_id = PpsParser::ParsePpsIdFromSlice(nalu_payload, payload_size);
            if (pps_id.has_value()) {
                nalu_info.pps_id = pps_id.value();
            } else {
                PLOG_WARNING << "Failed to parse PPS from slice of type=" << nalu_info.type;
            }
            break;
        }
        // Slices below don't contains SPS or PPS ids, ignoring.
        case h264::NaluType::AUD:
        case h264::NaluType::END_OF_SEQUENCE:
        case h264::NaluType::END_OF_STREAM:
        case h264::NaluType::FILLER:
        case h264::NaluType::SEI:
            break;
        case h264::NaluType::STAP_A:
        case h264::NaluType::FU_A:
        {
            PLOG_WARNING << "Unexpected STAP-A or FU-A packet received.";
            return false;
        }   
        default:
            break;
        }

        if (h264_video_codec_header.nalus.size() == h264::kMaxNaluNumPerPacket) {
            PLOG_WARNING << "Received packet containing more than "
                         << h264::kMaxNaluNumPerPacket
                         << " NAL units. Will not keep track sps and pps ids for all of them.";
        } else {
            h264_video_codec_header.nalus.push_back(std::move(nalu_info));
        }
        return true;
    };

    uint8_t nal_type = payload_data[0] & kTypeMask;
    if (nal_type == h264::NaluType::STAP_A) {
        // Skip the StapA header
        if (rtp_payload.size() <= kStapAHeaderSize) {
            PLOG_WARNING << "StapA header truncated.";
            return std::nullopt;
        }

        auto nalu_indices = ParseNaluIndicesPerStapAPacket(payload_data, rtp_payload.size());
        if (!nalu_indices.has_value()) {
            PLOG_WARNING << "StapA packet with incorrect NALU packet lengths.";
            return std::nullopt;
        }

        h264_video_codec_header.packetization_type = h264::PacketizationType::STAP_A;
        // NALU type for aggregated packets is the type of the first packet only.
        h264_video_codec_header.packet_nalu_type = payload_data[kStapAHeaderSize] & kTypeMask;

        // Parse NAL units in STAP-A packet
        for (const auto& nalu_index : nalu_indices.value()) {
            if (!nalu_parser(nalu_index.payload_start_offset, nalu_index.payload_size)) {
                return std::nullopt;
            }
        }
    } else {
        h264_video_codec_header.packetization_type = h264::PacketizationType::SIGNLE;
        // The NAL unit type of the original data for fragmented packet
        h264_video_codec_header.packet_nalu_type = nal_type;
        if (!nalu_parser(0, payload_size)) {
            return std::nullopt;
        }
    }
    return depacketized_payload;
}

// Fragment payload into packets (FU-A)
// e.g.: RTP payload format for FU-A
// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | FU indicator  |   FU header   |                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
// |                                                               |
// |                         FU payload                            |
// |                                                               |
// |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                               :...OPTIONAL RTP padding        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

// FU indicator, RFC 6184, Section 5.3
// +---------------+
// |0|1|2|3|4|5|6|7|
// +-+-+-+-+-+-+-+-+
// |F|NRI|  Type   |
// +---------------+

// FU header, RFC 6184, Section 5.8
// +---------------+
// |0|1|2|3|4|5|6|7|
// +-+-+-+-+-+-+-+-+
// |S|E|R|  Type   |
// +---------------+
std::optional<RtpDepacketizer::Packet> RtpH264Depacketizer::DepacketizeFuANalu(CopyOnWriteBuffer rtp_payload) {
    if (rtp_payload.size() < kFuAHeaderSize) {
        PLOG_WARNING << "FU-A NAL units is truncted.";
        return std::nullopt;
    }
    Packet depacketized_payload;
    const uint8_t* rtp_payload_data = rtp_payload.cdata();
    uint8_t fnri = rtp_payload_data[0] & (kFBit | kNriMask);
    uint8_t original_nal_type = rtp_payload_data[1] & kTypeMask;
    bool is_first_fragment = (rtp_payload_data[1] & kSBit) > 0;
    h264::NaluInfo nalu_info;
    nalu_info.type = original_nal_type;
    nalu_info.pps_id = -1;
    nalu_info.sps_id = -1;
    // First packet in frame.
    if (is_first_fragment) {
        std::optional<uint32_t> pps_id = PpsParser::ParsePpsIdFromSlice(rtp_payload.cdata() + 2 * kNalHeaderSize,
                                                                         rtp_payload.size() - 2 * kNalHeaderSize);
        if (pps_id.has_value()) {
            nalu_info.pps_id = pps_id.value();
        } else {
            PLOG_WARNING << "Failed to parse PPS from first fragment of FU-A NAL unit with original type="
                         << static_cast<int>(nalu_info.type);
        }
        uint8_t original_nal_header = fnri | original_nal_type;
        depacketized_payload.video_payload = CopyOnWriteBuffer(rtp_payload.data() + kNalHeaderSize, rtp_payload.size() - kNalHeaderSize);
        depacketized_payload.video_payload.data()[0] = original_nal_header;
    } else {
        depacketized_payload.video_payload = CopyOnWriteBuffer(rtp_payload.data() + kFuAHeaderSize, rtp_payload.size() - kFuAHeaderSize);
    }

    bool is_idr = original_nal_type == h264::NaluType::IDR;
    // Frame type
    depacketized_payload.video_header.frame_type = is_idr ? video::FrameType::KEY 
                                                           : video::FrameType::DELTA;
    depacketized_payload.video_header.frame_width = 0;
    depacketized_payload.video_header.frame_height = 0;
    depacketized_payload.video_header.codec_type = video::CodecType::H264;
    depacketized_payload.video_header.is_first_packet_in_frame = is_first_fragment;
    // H264 packetization info
    auto& h264_video_codec_header = depacketized_payload.video_codec_header.emplace<h264::PacketizationInfo>();
    h264_video_codec_header.packetization_type = h264::PacketizationType::FU_A;
    h264_video_codec_header.packetization_mode = h264::PacketizationMode::NON_INTERLEAVED;
    h264_video_codec_header.packet_nalu_type = original_nal_type;
    h264_video_codec_header.has_idr = is_idr;
    if (is_first_fragment) {
        h264_video_codec_header.nalus.push_back(std::move(nalu_info));
    }
    return depacketized_payload;
}

} // namespace naivertc