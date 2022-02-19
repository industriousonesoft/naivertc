#ifndef _RTC_RTP_RTCP_RTP_FEC_FEC_DECODER_H_
#define _RTC_RTP_RTCP_RTP_FEC_FEC_DECODER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_codec.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"
#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_header_reader_ulp.hpp"

#include <map>
#include <memory>
#include <functional>

namespace naivertc {

class FecDecoder : public FecCodec {
public:
    // MediaPacket
    struct MediaPacket {
        uint32_t ssrc = 0;
        uint16_t seq_num = 0;

        CopyOnWriteBuffer pkt;
    };
    using ProtectedMediaPackets = std::map<uint16_t, MediaPacket, wrap_around_utils::OlderThan<uint16_t>>;

    // FecPacket
    struct FecPacket {
        // RTP header fields
        uint32_t ssrc = 0;
        uint16_t seq_num = 0;

        uint32_t protected_ssrc = 0;

        // FEC Header
        FecHeader fec_header;

        // FEC header + payload
        CopyOnWriteBuffer pkt;

        // List of media packets that this FEC packet protects.
        ProtectedMediaPackets protected_media_packets;
    };
    using ReceivedFecPackets = std::map<uint16_t, FecPacket, wrap_around_utils::OlderThan<uint16_t>>;

    // RecoveredMediaPacket 
    struct RecoveredMediaPacket : public MediaPacket {
        // Will be true if this packet was recovered by
        // the FEC. Otherwise it was a media packet passed in
        // through the received packet list.
        bool was_recovered;
    };
    using RecoveredMediaPackets = std::map<uint16_t, RecoveredMediaPacket, wrap_around_utils::OlderThan<uint16_t>>;

    // Convenient API to create FEC decoder.
    static std::unique_ptr<FecDecoder> CreateUlpFecDecoder(uint32_t ssrc);
    
public:
    ~FecDecoder() override;

    void Decode(uint32_t fec_ssrc, uint16_t seq_num, bool is_fec, CopyOnWriteBuffer received_packet);

    void Reset();

    using PacketRecoveredCallback = std::function<void(const RecoveredMediaPacket& recovered_packet)>;
    void OnRecoveredPacket(PacketRecoveredCallback callback);

private:
    FecDecoder(uint32_t fec_ssrc, uint32_t protected_media_ssrc, std::unique_ptr<FecHeaderReader> fec_header_reader);

    void InsertPacket(uint32_t fec_ssrc, uint16_t seq_num, bool is_fec, CopyOnWriteBuffer received_packet);
    void InsertFecPacket(uint32_t fec_ssrc, uint16_t seq_num, CopyOnWriteBuffer received_packet);
    void InsertMediaPacket(uint32_t media_ssrc, uint16_t seq_num, CopyOnWriteBuffer received_packet);

    void UpdateConveringFecPackets(const RecoveredMediaPacket& recovered_packet);
    void DiscardOldRecoveredPackets();
    void DiscardOldReceivedFecPackets();

    size_t NumPacketsToRecover(const FecPacket& fec_packet) const;
    void TryToRecover();
    std::optional<RecoveredMediaPacket> RecoverPacket(const FecPacket& fec_packet);
    std::optional<RecoveredMediaPacket> PreparePacketForRecovery(const FecPacket& fec_packet);
    bool FinishPacketForRecovery(RecoveredMediaPacket& recovered_packet);

    bool IsOldFecPacket(const FecPacket& fec_packet) const;
    
private:
    const uint32_t fec_ssrc_;
    const uint32_t protected_media_ssrc_;
    std::unique_ptr<FecHeaderReader> fec_header_reader_;

    ReceivedFecPackets received_fec_packets_;
    RecoveredMediaPackets recovered_media_packets_;

    PacketRecoveredCallback packet_recovered_callback_ = nullptr;
};

} // namespace naivertc

#endif