#include "rtc/channels/data_channel.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace message {

// Messages for the datachannel establishment protocol (RFC 8832)
// See https://tools.ietf.org/html/rfc8832

// https://datatracker.ietf.org/doc/html/rfc8832#section-8.2.1
/*
+===================+===========+===========+
| Name              | Type      | Reference |
+===================+===========+===========+
| Reserved          | 0x00      | RFC 8832  |
+-------------------+-----------+-----------+
| Reserved          | 0x01      | RFC 8832  |
+-------------------+-----------+-----------+
| DATA_CHANNEL_ACK  | 0x02      | RFC 8832  |
+-------------------+-----------+-----------+
| DATA_CHANNEL_OPEN | 0x03      | RFC 8832  |
+-------------------+-----------+-----------+
| Unassigned        | 0x04-0xfe |           |
+-------------------+-----------+-----------+
| Reserved          | 0xff      | RFC 8832  |
+-------------------+-----------+-----------+
*/

enum class Type : uint8_t {
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
 0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Message Type |  Channel Type |            Priority           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Reliability Parameter                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Label Length          |       Protocol Length         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
\                                                               /
|                             Label                             |
/                                                               \
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
\                                                               /
|                            Protocol                           |
/                                                               \
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
struct Open {
    // Fixed part
    uint8_t type = uint8_t(Type::OPEN);
    uint8_t channel_type;
    uint16_t priority;
    uint32_t reliability_parameter;
    uint16_t label_length;
    uint16_t protocol_length;
    // Variable part
    // uint8_t[label_length] label;
    // uint8_t[protocol_length] protocol;
};

// Ack Message
struct Ack {
    uint8_t type = uint8_t(Type::ACK);
};

// Close Message
struct Close {
    uint8_t type = uint8_t(Type::CLOSE);
};

#pragma pack(pop)

} // namespace message

bool DataChannel::IsOpenMessage(const CopyOnWriteBuffer& message) {
    if (message.empty()) {
        return false;
    }
    return message::Type(message.data()[0]) == message::Type::OPEN;
}

bool DataChannel::IsCloseMessage(const CopyOnWriteBuffer& message) {
    if (message.empty()) {
        return false;
    }
    return message::Type(message.data()[0]) == message::Type::CLOSE;
}

bool DataChannel::IsAckMessage(const CopyOnWriteBuffer& message) {
    if (message.empty()) {
        return false;
    }
    return message::Type(message.data()[0]) == message::Type::ACK;
}

void DataChannel::ParseOpenMessage(const CopyOnWriteBuffer& message, DataChannel::Init& init_config) {
    const uint8_t* message_data = message.data();
    const size_t message_size = message.size();
    if (message_size < sizeof(message::Open)) {
        throw std::invalid_argument("DataChannel open message too small");
    }
    message::Open open_msg = *reinterpret_cast<const message::Open*>(message_data);
    open_msg.priority = ntohs(open_msg.priority);
    open_msg.reliability_parameter = ntohl(open_msg.reliability_parameter);
    open_msg.label_length = ntohs(open_msg.label_length);
    open_msg.protocol_length = ntohs(open_msg.protocol_length);

    if (message_size < sizeof(message::Open) + size_t(open_msg.label_length + open_msg.protocol_length)) {
        throw std::invalid_argument("DataChannel open message truncated");
    }

    auto open_msg_variable_part = reinterpret_cast<const char*>(message_data + sizeof(message::Open));
    init_config.label.assign(open_msg_variable_part, open_msg.label_length);
    init_config.protocol.assign(open_msg_variable_part + open_msg.label_length, open_msg.protocol_length);

    // Negotiate the reliability policy
    init_config.ordered = (open_msg.channel_type & 0x80) == 0;
    message::ChannelType channel_type = message::ChannelType(open_msg.channel_type & 0x7f);
    switch (channel_type) {
        case message::ChannelType::PARTIAL_RELIABLE_REXMIT: {
            init_config.max_rtx_count = uint16_t(open_msg.reliability_parameter);
            break;
        }
        case message::ChannelType::PARTIAL_RELIABLE_TIMED: {
            init_config.max_rtx_ms = uint16_t(open_msg.reliability_parameter);
            break;
        }
        default: {
            init_config.max_rtx_count = std::nullopt;
            init_config.max_rtx_count = std::nullopt;
            break;
        }
    }
}

const CopyOnWriteBuffer DataChannel::CreateOpenMessage(const DataChannel::Init& init_config) {
    
    uint8_t channel_type;
    uint32_t reliability_parameter;
    if (init_config.max_rtx_count.has_value()) {
        channel_type = uint8_t(message::ChannelType::PARTIAL_RELIABLE_REXMIT);
        reliability_parameter = uint32_t(init_config.max_rtx_count.value());
    }else if (init_config.max_rtx_ms.has_value()) {
        channel_type = uint8_t(message::ChannelType::PARTIAL_RELIABLE_TIMED);
        reliability_parameter = uint32_t(init_config.max_rtx_ms.value());
    }else {
        channel_type = uint8_t(message::ChannelType::RELIABLE);
        reliability_parameter = 0;
    }

    if (init_config.ordered == false) {
        // unorder marker bit
        channel_type |= 0x80;
    }

    const auto& label = init_config.label;
    const auto& protocol = init_config.protocol;
    const size_t len = sizeof(message::Open) + label.size() + protocol.size();
    CopyOnWriteBuffer message(len);
    auto &open_msg_fixed_part = *reinterpret_cast<message::Open*>(message.data());
    open_msg_fixed_part.type = uint8_t(message::Type::OPEN);
    open_msg_fixed_part.channel_type = channel_type;
    // TODO: Config priority
    open_msg_fixed_part.priority = htons(0);
    open_msg_fixed_part.reliability_parameter = reliability_parameter;
    open_msg_fixed_part.label_length = htons(uint16_t(label.size()));
    open_msg_fixed_part.protocol_length = htons(uint16_t(protocol.size()));

    auto open_msg_variable_part = reinterpret_cast<char*>(message.data() + sizeof(open_msg_fixed_part));
    std::copy(label.begin(), label.end(), open_msg_variable_part);
    std::copy(protocol.begin(), protocol.end(), open_msg_variable_part + label.size());

    return message;
}

const CopyOnWriteBuffer DataChannel::CreateAckMessage() {
    CopyOnWriteBuffer message(sizeof(message::Ack));
    auto &ack_msg = *reinterpret_cast<message::Ack *>(message.data());
    ack_msg.type = uint8_t(message::Type::ACK);
    return message;
}

const CopyOnWriteBuffer DataChannel::CreateCloseMessage() {
    CopyOnWriteBuffer message(sizeof(message::Close));
    auto &ack_msg = *reinterpret_cast<message::Close *>(message.data());
    ack_msg.type = uint8_t(message::Type::CLOSE);
    return message;
}
    
} // namespace naivertc
