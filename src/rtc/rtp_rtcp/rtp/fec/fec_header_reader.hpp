#ifndef _RTC_RTP_RTCP_RTP_FEC_FEC_HEADER_READER_H_
#define _RTC_RTP_RTCP_RTP_FEC_FEC_HEADER_READER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"

namespace naivertc {

class FecHeaderReader {
public:
    virtual ~FecHeaderReader();

    // The maximum number of media packets that can be covered by one FEC packet.
    size_t max_media_packets() const { return max_media_packets_; };

    // The maximum number of FEC packets that is supported, per call
    // to ForwardErrorCorrection::EncodeFec().
    size_t max_fec_packets() const { return max_fec_packets_; }

    // Parses FEC header and stores information in ReceivedFecPacket members.
    virtual bool ReadFecHeader(FecHeader& fec_header, CopyOnWriteBuffer& fec_packet) const = 0;

protected:
    FecHeaderReader(size_t max_media_packets, size_t max_fec_packets);

    const size_t max_media_packets_;
    const size_t max_fec_packets_;
};

} // namespace naivertc

#endif