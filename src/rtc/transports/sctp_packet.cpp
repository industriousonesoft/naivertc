#include "rtc/transports/sctp_packet.hpp"

namespace naivertc {

SctpPacket::SctpPacket(const std::byte* data, size_t size, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability) 
    : Packet(std::move(data), size),
    type_(type), 
    stream_id_(stream_id),
    reliability_(reliability) {}

SctpPacket::SctpPacket(std::vector<std::byte>&& bytes, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability) 
    : Packet(std::move(bytes)),
    type_(type), 
    stream_id_(stream_id),
    reliability_(reliability) {}

SctpPacket::~SctpPacket() {}

}