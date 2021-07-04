#include "pc/transports/sctp_message.hpp"

namespace naivertc {

SctpMessage::SctpMessage(const std::byte* data, size_t size, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability) 
    : Packet(data, size),
    type_(type), 
    stream_id_(stream_id),
    reliability_(reliability) {}

SctpMessage::SctpMessage(std::vector<std::byte>&& bytes, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability) 
    : Packet(std::move(bytes)),
    type_(type), 
    stream_id_(stream_id),
    reliability_(reliability) {}

SctpMessage::~SctpMessage() {}

}