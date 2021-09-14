#include "rtc/transports/sctp_message.hpp"

namespace naivertc {

SctpMessage::SctpMessage(size_t capacity, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability) 
    : Packet(capacity),
      type_(type), 
      stream_id_(stream_id),
      reliability_(reliability) {}

SctpMessage::SctpMessage(const uint8_t* data, size_t size, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability) 
    : Packet(data, size),
      type_(type), 
      stream_id_(stream_id),
      reliability_(reliability) {}

SctpMessage::SctpMessage(const BinaryBuffer& buffer, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability) 
    : Packet(buffer),
      type_(type), 
      stream_id_(stream_id),
      reliability_(reliability) {}

SctpMessage::SctpMessage(BinaryBuffer&& buffer, Type type, StreamId stream_id, std::shared_ptr<Reliability> reliability) 
    : Packet(std::move(buffer)),
      type_(type), 
      stream_id_(stream_id),
      reliability_(reliability) {}

SctpMessage::~SctpMessage() {}

} // namespace naivertc