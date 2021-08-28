#include "rtc/rtp_rtcp/rtp/fec/fec_header_writer.hpp"

namespace naivertc {

FecHeaderWriter::FecHeaderWriter(size_t max_media_packets,
                                 size_t max_fec_packets,
                                 size_t max_packet_overhead)
    : max_media_packets_(max_media_packets),
      max_fec_packets_(max_fec_packets),
      max_packet_overhead_(max_packet_overhead) {}

FecHeaderWriter::~FecHeaderWriter() = default;

size_t FecHeaderWriter::max_media_packets() const {
    return max_media_packets_;
}

size_t FecHeaderWriter::max_fec_packets() const {
    return max_fec_packets_;
}

size_t FecHeaderWriter::max_packet_overhead() const {
    return max_packet_overhead_;
}
    
} // namespace naivertc
