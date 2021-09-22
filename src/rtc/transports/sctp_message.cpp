#include "rtc/transports/sctp_message.hpp"

namespace naivertc {

// SctpMessage
SctpMessage::SctpMessage(Type type, 
                         uint16_t stream_id,
                         CopyOnWriteBuffer payload) 
    : type_(type), 
      stream_id_(stream_id),
      payload_(std::move(payload)) {}

SctpMessage::~SctpMessage() {}


// SctpMessageToSend
SctpMessageToSend::SctpMessageToSend(Type type, 
                                     uint16_t stream_id,
                                     CopyOnWriteBuffer payload,
                                     const Reliability& reliability)
    : SctpMessage(type, stream_id, std::move(payload)),
      reliability_(reliability),
      offset_(0) {}

SctpMessageToSend::~SctpMessageToSend() {}

size_t SctpMessageToSend::available_payload_size() const {
    return payload_.size() - offset_;
}

const uint8_t* SctpMessageToSend::available_payload_data() const {
    return payload_.data() + offset_;
}

void SctpMessageToSend::Advance(size_t increment) {
    size_t new_offset = offset_ + increment;
    if (new_offset > payload_.size()) {
        new_offset = payload_.size();
    }
    offset_ = new_offset;
}

// operator <<
std::ostream& operator<<(std::ostream &out, SctpMessage::Type type) {
    using Type = SctpMessage::Type;
    switch(type) {
    case Type::CONTROL:
      out << "control";
      break;
    case Type::BINARY:
      out << "binary";
      break;
    case Type::STRING:
      out << "string";
      break;
    }
    return out;
}

} // namespace naivertc