#ifndef _RTC_RTP_RTCP_RTCP_PACKETS_SENDER_REPORT_H_
#define _RTC_RTP_RTCP_RTCP_PACKETS_SENDER_REPORT_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"
#include "rtc/base/time/ntp_time.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/report_block.hpp"

#include <vector>

namespace naivertc {
namespace rtcp {

class CommonHeader;

class RTC_CPP_EXPORT SenderReport : public RtcpPacket {
public:
    static constexpr uint8_t kPacketType = 200;
    static constexpr size_t kMaxNumberOfReportBlocks = 0x1F;

    SenderReport();
    SenderReport(const SenderReport&);
    SenderReport(SenderReport&&);
    SenderReport& operator=(const SenderReport&);
    SenderReport& operator=(SenderReport&&);
    ~SenderReport() override;

    NtpTime ntp() const { return ntp_; }
    uint32_t rtp_timestamp() const { return rtp_timestamp_; }
    uint32_t sender_packet_count() const { return sender_packet_count_; }
    uint32_t sender_octet_count() const { return sender_octet_count_; }

    void set_ntp(NtpTime ntp) { ntp_ = ntp; }
    void set_rtp_timestamp(uint32_t rtp_timestamp) { rtp_timestamp_ = rtp_timestamp; }
    void set_sender_packet_count(uint32_t packet_count) { sender_packet_count_ = packet_count; }
    void set_sender_octet_count(uint32_t octet_count) { sender_octet_count_ = octet_count; }
    const std::vector<ReportBlock>& report_blocks() const {
        return report_blocks_;
    }

    bool AddReportBlock(const ReportBlock& block);
    bool SetReportBlocks(std::vector<ReportBlock> blocks);
    void ClearReportBlocks() { report_blocks_.clear(); }

    // Parse assumes header is already parsed and validated.
    bool Parse(const CommonHeader& packet);

    // Size of this packet in bytes including headers
    size_t PacketSize() const override;

    // Pack data into the given buffer at the given position.
    bool PackInto(uint8_t* buffer,
                  size_t* index,
                  size_t max_size,
                  PacketReadyCallback callback) const override;

private:
    static constexpr size_t kSenderReportFixedSize = 24;

    NtpTime ntp_;
    uint32_t rtp_timestamp_;
    uint32_t sender_packet_count_;
    uint32_t sender_octet_count_;
    std::vector<ReportBlock> report_blocks_;
};
    
} // namespace rtcp
} // namespace naivertc


#endif