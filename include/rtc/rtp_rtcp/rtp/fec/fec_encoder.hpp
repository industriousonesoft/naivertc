#ifndef _RTC_RTP_RTCP_FEC_ENCODER_H_
#define _RTC_RTP_RTCP_FEC_ENCODER_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_header_writer.hpp"

#include <list>

namespace naivertc {

class RTC_CPP_EXPORT FecEncoder {
public:
    // Using static Create method to make sure the FEC coder is unique, 
    // and not allowed to share with others
    static std::unique_ptr<FecEncoder> CreateUlpfecEncoder();

    using PacketList = std::list<std::shared_ptr<FecPacket>>;
    using PacketViewList = std::list<std::shared_ptr<FecPacketView>>;
public:
    ~FecEncoder();

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
    bool Encode(const PacketViewList& media_packets, 
                uint8_t protection_factor, 
                size_t num_important_packets, 
                bool use_unequal_protection, 
                FecMaskType fec_mask_type);

    static size_t NumFecPackets(size_t num_media_packets, uint8_t protection_factor);

protected:
    FecEncoder(std::unique_ptr<FecHeaderWriter> fec_header_writer);

private:
    enum class ProtectionMode {
        NO_OVERLAP, 
        OVERLAP,
        BIAS_FIRST_PACKET,
        DEAULE = OVERLAP
    };
    // PacketMaskTable
    class PacketMaskTable {
    public:
        PacketMaskTable(FecMaskType fec_mask_type, size_t num_media_packets);
        ~PacketMaskTable();

        ArrayView<const uint8_t> LookUp(size_t num_media_packets, size_t num_fec_packets);

        // Returns packet masks of a single FEC packet, 
        // The mask indicates which media packts should be protected by the FEC packet.
        bool GeneratePacketMasks(size_t num_media_packets,
                                 size_t num_fec_packets,
                                 size_t num_imp_packets,
                                 bool use_unequal_protection,
                                 uint8_t* packet_masks);
    private:
        void GenerateUnequalProtectionMasks(size_t num_media_packets,
                                            size_t num_fec_packets,
                                            size_t num_imp_packets,
                                            size_t num_mask_bytes,
                                            uint8_t* packet_masks);

        void GenerateImportantProtectionMasks(size_t num_fec_for_imp_packets,
                                              size_t num_imp_packets,
                                              size_t num_mask_bytes,
                                              uint8_t* packet_masks);

        void GenerateRemainingProtectionMasks(size_t num_media_packets,
                                              size_t num_fec_remaining,
                                              size_t num_fec_for_imp_packets,
                                              size_t num_mask_bytes,
                                              ProtectionMode mode,
                                              uint8_t* packet_masks);     

        size_t NumberOfFecPacketForImportantPackets(size_t num_media_packets,
                                                    size_t num_fec_packets,
                                                    size_t num_important_packets);

    private:
        // Returns the required packet mask size
        static size_t PacketMaskSize(size_t num_packets);

        static const uint8_t* PickTable(FecMaskType fec_mask_type, 
                                        size_t num_media_packets);

        static void FitSubMasks(size_t num_mask_bytes, 
                                size_t num_sub_mask_bytes, 
                                size_t num_row, 
                                const uint8_t* sub_packet_masks, 
                                uint8_t* packet_masks);
    private:
        const uint8_t* table_;
        uint8_t fec_packet_masks_[kFECPacketMaskMaxSize];
    };

    static const uint8_t kPacketMaskBurstyTable[];
    static const uint8_t kPacketMaskRandomTable[];

    // The algorithm is tailored to look up data in the PacketMaskBurstyTable or PacketMaskRandomTable.
    // These tables only cover fec code for up to 12 media packets.
    static ArrayView<const uint8_t> LookUpInFecTable(const uint8_t* table, size_t media_packet_index, size_t fec_packet_index);

    

private:
    std::unique_ptr<FecHeaderWriter> fec_header_writer_;
    std::vector<FecPacket> generated_fec_packets_;
};
    
} // namespace naivertc


#endif