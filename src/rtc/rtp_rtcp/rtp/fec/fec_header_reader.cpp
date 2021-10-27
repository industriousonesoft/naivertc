#include "rtc/rtp_rtcp/rtp/fec/fec_header_reader.hpp"

namespace naivertc {

FecHeaderReader::FecHeaderReader(size_t max_media_packets, size_t max_fec_packets) 
: max_media_packets_(max_media_packets),
  max_fec_packets_(max_fec_packets) {}

FecHeaderReader::~FecHeaderReader() = default;

} // namespace naivertc