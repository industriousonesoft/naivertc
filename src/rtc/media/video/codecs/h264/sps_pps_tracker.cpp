#include "rtc/media/video/codecs/h264/sps_pps_tracker.hpp"
#include "rtc/media/video/codecs/h264/sps_parser.hpp"
#include "rtc/media/video/codecs/h264/pps_parser.hpp"

#include <plog/Log.h>

#include <variant>

namespace naivertc {
namespace h264 {

namespace {
const uint8_t start_code_h264[] = {0, 0, 0, 1};
}  // namespace

SpsPpsTracker::SpsPpsTracker() = default;
SpsPpsTracker::~SpsPpsTracker() = default;

SpsPpsTracker::PpsInfo::PpsInfo() = default;
SpsPpsTracker::PpsInfo::PpsInfo(PpsInfo&& rhs) = default;
SpsPpsTracker::PpsInfo& SpsPpsTracker::PpsInfo::operator=(
    PpsInfo&& rhs) = default;
SpsPpsTracker::PpsInfo::~PpsInfo() = default;

SpsPpsTracker::SpsInfo::SpsInfo() = default;
SpsPpsTracker::SpsInfo::SpsInfo(SpsInfo&& rhs) = default;
SpsPpsTracker::SpsInfo& SpsPpsTracker::SpsInfo::operator=(
    SpsInfo&& rhs) = default;
SpsPpsTracker::SpsInfo::~SpsInfo() = default;

SpsPpsTracker::FixedBitstream SpsPpsTracker::CopyAndFixBitstream(bool is_first_packet_in_frame,
                                                                         uint16_t& fixed_frame_width,
                                                                         uint16_t& fixed_frame_height, 
                                                                         h264::PacketizationInfo& h264_header,
                                                                         ArrayView<const uint8_t> bitstream) {
    assert(bitstream.size() > 0);

    bool append_sps_pps = false;
    auto sps = sps_data_.end();
    auto pps = pps_data_.end();

    for (const auto& nalu : h264_header.nalus) {
        switch (nalu.type) {
        case h264::NaluType::SPS: {
            SpsInfo& sps_info = sps_data_[nalu.sps_id];
            sps_info.width = fixed_frame_width;
            sps_info.height = fixed_frame_height;
            break;
        }
        case h264::NaluType::PPS: {
            pps_data_[nalu.pps_id].sps_id = nalu.sps_id;
            break;
        }
        case h264::NaluType::IDR: {
            // If this is the first packet of an IDR, make sure we have the required
            // SPS/PPS and also calculate how much extra space we need in the buffer
            // to prepend the SPS/PPS to the bitstream with start codes.
            if (is_first_packet_in_frame) {
                if (nalu.pps_id == -1) {
                    PLOG_WARNING << "No PPS id in IDR nalu.";
                    return {PacketAction::REQUEST_KEY_FRAME};
                }

                pps = pps_data_.find(nalu.pps_id);
                if (pps == pps_data_.end()) {
                    PLOG_WARNING << "No PPS with id << " << nalu.pps_id << " received";
                    return {PacketAction::REQUEST_KEY_FRAME};
                }

                sps = sps_data_.find(pps->second.sps_id);
                if (sps == sps_data_.end()) {
                    PLOG_WARNING << "No SPS with id << " << pps->second.sps_id << " received";
                    return {PacketAction::REQUEST_KEY_FRAME};
                }

                // Since the first packet of every keyframe should have its width and
                // height, so we set it here in the case of it being supplied out of
                // band.
                fixed_frame_width = sps->second.width;
                fixed_frame_height = sps->second.height;

                // If the SPS/PPS was supplied out of band then we will have saved
                // the actual bitstream in |data|.
                if (sps->second.data && pps->second.data) {
                    assert(sps->second.size > 0);
                    assert(pps->second.size > 0);
                    append_sps_pps = true;
                }
            }
            break;
        }
        default:
            break;
        }
    }

    assert(!append_sps_pps || (sps != sps_data_.end() && pps != pps_data_.end()));

    // Calculate how much space we need for the rest of the bitstream.
    size_t required_size = 0;

    if (append_sps_pps) {
        required_size += sps->second.size + sizeof(start_code_h264);
        required_size += pps->second.size + sizeof(start_code_h264);
    }

    if (h264_header.packetization_type == h264::PacketizationType::STAP_A) {
        // Skip the Stap-A header (1 byte)
        const uint8_t* nalu_ptr = bitstream.data() + 1;
        while (nalu_ptr < bitstream.data() + bitstream.size() - 1) {
            assert(is_first_packet_in_frame && "STAP-A is always the first packet in frame.");
            required_size += sizeof(start_code_h264);

            // The first two bytes describe the length of a segment.
            uint16_t segment_length = nalu_ptr[0] << 8 | nalu_ptr[1];
            nalu_ptr += 2;

            required_size += segment_length;
            nalu_ptr += segment_length;
        }
    } else {
        if (h264_header.nalus.size() > 0) {
            required_size += sizeof(start_code_h264);
        }
        required_size += bitstream.size();
    }

    // Then we copy to the new buffer.
    SpsPpsTracker::FixedBitstream fixed;
    fixed.bitstream.EnsureCapacity(required_size);

    if (append_sps_pps) {
        // Insert SPS.
        fixed.bitstream.Append(start_code_h264);
        fixed.bitstream.Append(sps->second.data.get(), sps->second.size);

        // Insert PPS.
        fixed.bitstream.Append(start_code_h264);
        fixed.bitstream.Append(pps->second.data.get(), pps->second.size);

        // Update codec header to reflect the newly added SPS and PPS.
        h264::NaluInfo sps_info;
        sps_info.type = h264::NaluType::SPS;
        sps_info.sps_id = sps->first;
        sps_info.pps_id = -1;
        h264::NaluInfo pps_info;
        pps_info.type = h264::NaluType::PPS;
        pps_info.sps_id = sps->first;
        pps_info.pps_id = pps->first;
        if (h264_header.nalus.size() + 2 <= h264::kMaxNaluNumPerPacket) {
            h264_header.nalus.push_back(std::move(sps_info));
            h264_header.nalus.push_back(std::move(pps_info));
        } else {
            PLOG_WARNING << "Not enough space in H.264 codec header to insert "
                            "SPS/PPS provided out-of-band.";
        }
    }

    // Copy the rest of the bitstream and insert start codes.
    // STAP-A
    if (h264_header.packetization_type == h264::PacketizationType::STAP_A) {
        // Stap-A: STAP-A NAL HDR + NALU 1 Size + NALU 1 HDR ... 
        // Skip the Stap-A NAL header (1 byte)
        const uint8_t* payload_data = bitstream.data();
        const size_t payload_size = bitstream.size();
        size_t offset = 1;
        while (offset < payload_size) {
            // Append the start code bytes
            fixed.bitstream.Append(start_code_h264);

            // The first two bytes describe the length of a nalu.
            uint16_t nalu_size = payload_data[offset] << 8 | payload_data[offset + 1];
            offset += 2;

            if (offset + nalu_size > payload_size) {
                PLOG_WARNING << "STAP-A packet truncated.";
                return {PacketAction::DROP};
            }

            fixed.bitstream.Append(&payload_data[offset], nalu_size);
            offset += nalu_size;
        }
    // Single or FU-A packet
    } else {
        // Has NAL unit begin in the packet
        if (h264_header.nalus.size() > 0) {
            fixed.bitstream.Append(start_code_h264);
        }
        fixed.bitstream.Append(bitstream.data(), bitstream.size());
    }

    fixed.action = PacketAction::INSERT;
    return fixed;
}

void SpsPpsTracker::InsertSpsPpsNalus(const std::vector<uint8_t>& sps,
                                          const std::vector<uint8_t>& pps) {
    constexpr size_t kNaluHeaderOffset = 1;
    if (sps.size() < kNaluHeaderOffset) {
        PLOG_WARNING << "SPS size  " << sps.size() << " is smaller than "
                            << kNaluHeaderOffset;
        return;
    }
    if ((sps[0] & 0x1f) != h264::NaluType::SPS) {
        PLOG_WARNING << "SPS Nalu header missing";
        return;
    }
    if (pps.size() < kNaluHeaderOffset) {
        PLOG_WARNING << "PPS size  " << pps.size() << " is smaller than "
                            << kNaluHeaderOffset;
        return;
    }
    if ((pps[0] & 0x1f) != h264::NaluType::PPS) {
        PLOG_WARNING << "SPS Nalu header missing";
        return;
    }
    std::optional<SpsParser::SpsState> parsed_sps = SpsParser::ParseSps(
        sps.data() + kNaluHeaderOffset, sps.size() - kNaluHeaderOffset);
    std::optional<PpsParser::PpsState> parsed_pps = PpsParser::ParsePps(
        pps.data() + kNaluHeaderOffset, pps.size() - kNaluHeaderOffset);

    if (!parsed_sps) {
        PLOG_WARNING << "Failed to parse SPS.";
    }

    if (!parsed_pps) {
        PLOG_WARNING << "Failed to parse PPS.";
    }

    if (!parsed_pps || !parsed_sps) {
        return;
    }

    SpsInfo sps_info;
    sps_info.size = sps.size();
    sps_info.width = parsed_sps->width;
    sps_info.height = parsed_sps->height;
    uint8_t* sps_data = new uint8_t[sps_info.size];
    memcpy(sps_data, sps.data(), sps_info.size);
    sps_info.data.reset(sps_data);
    sps_data_[parsed_sps->id] = std::move(sps_info);

    PpsInfo pps_info;
    pps_info.size = pps.size();
    pps_info.sps_id = parsed_pps->sps_id;
    uint8_t* pps_data = new uint8_t[pps_info.size];
    memcpy(pps_data, pps.data(), pps_info.size);
    pps_info.data.reset(pps_data);
    pps_data_[parsed_pps->id] = std::move(pps_info);

    PLOG_INFO << "Inserted SPS id " << parsed_sps->id << " and PPS id "
              << parsed_pps->id << " (referencing SPS "
              << parsed_pps->sps_id << ")";
}
    
} // namespace h264
} // namespace naivertc
