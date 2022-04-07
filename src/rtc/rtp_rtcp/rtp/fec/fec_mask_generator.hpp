#ifndef _RTC_RTP_RTCP_FEC_MASK_GENERATOR_H_
#define _RTC_RTP_RTCP_FEC_MASK_GENERATOR_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"

namespace naivertc {

class FecPacketMaskGenerator {
public:
    // UEP(Unequal protection) mode
    enum class UEPMode {
        // The masks for important media packets and normal media packets are not overlaped.
        NO_OVERLAP,
        // The masks for important media packets and normal media packets are overlaped.
        OVERLAP,
        // Based on the equal protection mode (means no important media packets here), 
        // and all the FEC packets will protect the first media packet.
        BIAS_FIRST_PACKET,
        // The default mode is overlap
        DEAULE = OVERLAP
    };
public:
    FecPacketMaskGenerator();
    ~FecPacketMaskGenerator();

    // Returns packet masks of a single FEC packet, 
    // The mask indicates which media packts should be protected by the FEC packet.
    bool GeneratePacketMasks(FecMaskType fec_mask_type,
                             size_t num_media_packets,
                             size_t num_fec_packets,
                             size_t num_imp_packets,
                             bool use_unequal_protection,
                             uint8_t* packet_masks);

    // Returns the required packet mask size
    
    static size_t PacketMaskSize(size_t num_packets);

private:
    void PickFixedMaskTable(FecMaskType fec_mask_type, size_t num_media_packets);

    void GenerateUnequalProtectionMasks(size_t num_media_packets,
                                        size_t num_fec_packets,
                                        size_t num_imp_packets,
                                        size_t num_mask_bytes,
                                        uint8_t* packet_masks,
                                        UEPMode mode = UEPMode::DEAULE/* Overlap */);

    void GenerateImportantProtectionMasks(size_t num_fec_for_imp_packets,
                                          size_t num_imp_packets,
                                          size_t num_mask_bytes,
                                          uint8_t* packet_masks);

    void GenerateRemainingProtectionMasks(size_t num_media_packets,
                                          size_t num_fec_remaining,
                                          size_t num_fec_for_imp_packets,
                                          size_t num_mask_bytes,
                                          UEPMode mode,
                                          uint8_t* packet_masks);     

    size_t NumberOfFecPacketForImportantPackets(size_t num_media_packets,
                                                size_t num_fec_packets,
                                                size_t num_important_packets);

    ArrayView<const uint8_t> LookUpPacketMasks(size_t num_media_packets, size_t num_fec_packets);
private:
    // The algorithm is tailored to look up data in the PacketMaskBurstyTable or PacketMaskRandomTable.
    // These tables only cover fec code for up to 12 media packets.
    ArrayView<const uint8_t> LookUpInFixedMaskTable(const uint8_t* mask_table, 
                                                    size_t num_media_packets, 
                                                    size_t num_fec_packets);

    static void FitSubMasks(size_t num_mask_bytes, 
                            size_t num_sub_mask_bytes, 
                            size_t num_row, 
                            const uint8_t* sub_packet_masks, 
                            uint8_t* packet_masks);

    static void ShiftFitSubMask(size_t num_mask_bytes, 
                                size_t num_sub_mask_bytes, 
                                size_t num_col_shift, 
                                size_t end_row, 
                                const uint8_t* sub_packet_masks, 
                                uint8_t* packet_masks);
private:
    const uint8_t* fixed_mask_table_;
    uint8_t fec_packet_masks_[kFECPacketMaskMaxSize];

    static const uint8_t kPacketMaskBurstyTable[];
    static const uint8_t kPacketMaskRandomTable[];
};
    
} // namespace naivertc


#endif