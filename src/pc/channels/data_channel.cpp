#include "pc/channels/data_channel.hpp"

namespace naivertc {

// Messages for the datachannel establishment protocol (RFC 8832)
// See https://tools.ietf.org/html/rfc8832

enum class MessageType : uint8_t {
    OPEN_REQUEST = 0x00,
    OPEN_RESPONSE = 0x01,
    ACK = 0x02,
    OPEN = 0x03,
    CLOSE = 0x04
};

// See https://datatracker.ietf.org/doc/html/draft-jesup-rtcweb-data-protocol-03
enum class ChannelType : uint8_t {
    // The channel provides a reliable bi-directional communication channel
    RELIABLE = 0x00,
    // The channel provides a partial reliable bi-directional communication channel.
    // User message will not be retransmitted more times than specified in the Reliability Parameter.
    PARTIAL_RELIABLE_REXMIT = 0x01,
    // The channel provides a partial reliable bi-directional communication channel.
    // User message might not be transmitted or retransmitted after a specified life-time given in milli-second
    // in the Reliability Parameter. 
    PARTIAL_RELIABLE_TIMED = 0x02
};

// pack(n) + pop() : 设置n字节对齐 + 恢复到编译器默认的对齐方式
// pack(push, 1) + pop(): 先把旧设置的对齐方式压栈，再使用当前设置的对齐方式 + 恢复到之前设置的对齐方式
#pragma pack(push, 1)

// Open Message
/** 
 * 0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |  Message Type |  Channel Type |           Flags               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |     Reliability Parameter     |          Priority             |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     \                                                               /
     |                             Label                             |
     /                                                               \
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
struct OpenMessage {
    uint8_t type = uint8_t(MessageType::OPEN);
    uint8_t channel_type;
    uint16_t priority;
    uint32_t reliability_parameter;
    uint16_t label_length;
    uint16_t protocol_length;
    // the following fields:
    // uint8_t[label_length] label;
    // uint8_t[protocol_length] protocol;
};

// Ack Message
struct AckMessage {
    uint8_t type = uint8_t(MessageType::ACK);
};

// Close Message
struct CloseMessage {
    uint8_t type = uint8_t(MessageType::CLOSE);
};

#pragma pack(pop)

// DataChannel
DataChannel::DataChannel(StreamId stream_id, std::string label, std::string protocol) 
    : stream_id_(stream_id),
    label_(label),
    protocol_(protocol) {
}

DataChannel::~DataChannel() {}

StreamId DataChannel::stream_id() const {
    return stream_id_;
}

std::string DataChannel::label() const {
    return label_;
}

std::string DataChannel::protocol() const {
    return protocol_;
}

void DataChannel::HintStreamIdForRole(sdp::Role role) {
    if (role == sdp::Role::ACTIVE) {
        if (stream_id_ % 2 == 1) {
            stream_id_ -= 1;
        }
    }else if (role == sdp::Role::PASSIVE) {
        if (stream_id_ % 2 == 0) {
            stream_id_ += 1;
        }
    }else {
        // This is ok
    }
}
    
} // namespace naivertc