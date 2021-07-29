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

void DataChannel::OnBufferedAmount(size_t amount) {
    TriggerBufferedAmount(amount);
}

void DataChannel::OnIncomingMessage(std::shared_ptr<SctpMessage> message) {
    if (!message) {
		return;
	}
    switch (message->type()) {
        case SctpMessage::Type::CONTROL: {
            if (message->size() == 0) {
                break;
            }
            auto message_type = message::Type(message->data()[0]);
            switch (message_type) {
            case message::Type::OPEN: {
                ProcessOpenMessage(message->data(), message->size());
                SendAckMessage();
                TriggerOpen();
                break;
            }
            case message::Type::ACK:
                TriggerOpen();
                break;
            case message::Type::CLOSE:
                recv_message_queue_.push(message);
                TriggerAvailable(recv_message_queue_.size());
                break;
            default:
                // Ignore
                break;
            }
            break;
        }
        case SctpMessage::Type::STRING: {
            recv_message_queue_.push(message);
            TriggerAvailable(recv_message_queue_.size());
            break;
        }
        case SctpMessage::Type::BINARY: {
            recv_message_queue_.push(message);
            TriggerAvailable(recv_message_queue_.size());
            break;
        }
        default:
            // Ignore
            break;
    }
}

void DataChannel::ProcessOpenMessage(const uint8_t* in_data, size_t in_size) {
    auto transport = sctp_transport_.lock();
    if (!transport) {
        throw std::runtime_error("DataChannel has no transport");
    }
    if (in_size < sizeof(message::Open)) {
        throw std::invalid_argument("DataChannel open message too small");
    }
    message::Open open_msg = *reinterpret_cast<const message::Open*>(in_data);
    open_msg.priority = ntohs(open_msg.priority);
    open_msg.reliability_parameter = ntohl(open_msg.reliability_parameter);
    open_msg.label_length = ntohs(open_msg.label_length);
    open_msg.protocol_length = ntohs(open_msg.protocol_length);

    if (in_size < sizeof(message::Open) + size_t(open_msg.label_length + open_msg.protocol_length)) {
        throw std::invalid_argument("DataChannel open message truncated");
    }

    auto open_msg_variable_part = reinterpret_cast<const char*>(in_data + sizeof(message::Open));
    label_.assign(open_msg_variable_part, open_msg.label_length);
    protocol_.assign(open_msg_variable_part + open_msg.label_length, open_msg.protocol_length);

    reliability_->unordered = (open_msg.channel_type & 0x80) != 0;
    message::ChannelType channel_type = message::ChannelType(open_msg.channel_type & 0x7f);
    switch (channel_type) {
        case message::ChannelType::PARTIAL_RELIABLE_REXMIT: {
            reliability_->policy = SctpMessage::Reliability::Policy::RTX;
            reliability_->rexmit = int(open_msg.reliability_parameter);
            break;
        }
        case message::ChannelType::PARTIAL_RELIABLE_TIMED: {
            reliability_->policy = SctpMessage::Reliability::Policy::TTL;
            reliability_->rexmit = std::chrono::milliseconds(open_msg.reliability_parameter);
            break;
        }
        default: {
            reliability_->policy = SctpMessage::Reliability::Policy::NONE;
            reliability_->rexmit = int(0);
            break;
        }
    }
}

void DataChannel::ProcessPendingMessages() {
    while(!recv_message_queue_.empty()) {
        auto message = recv_message_queue_.front();
        if (message->type() == SctpMessage::Type::BINARY) {
            if (binary_message_received_callback_) {
                binary_message_received_callback_(message->data(), message->size());
            }
        }else if (message->type() == SctpMessage::Type::STRING) {
            std::string text = std::string(message->data(), message->data() + message->size());
            if (text_message_received_callback_) {
                text_message_received_callback_(text);
            }else {
                PLOG_INFO << "Receive text: " << text;
            }
        }else {
            // Close message from remote peer
            if (!message->empty() && 
                message->type() == SctpMessage::Type::CONTROL &&
                message->data()[0] == uint8_t(message::Type::CLOSE)) {
                RemoteClose();
            }
        }
        recv_message_queue_.pop();
    }
}

bool DataChannel::IsOpenMessage(std::shared_ptr<SctpMessage> message) {
    if (message->type() != SctpMessage::Type::CONTROL) {
        return false;
    }
    auto message_type = message::Type(message->data()[0]);
    return message_type == message::Type::OPEN;
}

// Send
void DataChannel::SendOpenMessage() const {
    auto transport = sctp_transport_.lock();
    if (!transport) {
        throw std::logic_error("DataChannel has no transport");
    }
    uint8_t channel_type;
    uint32_t reliability_parameter;
    switch (reliability_->policy) {
        case SctpMessage::Reliability::Policy::RTX: {
            channel_type = uint8_t(message::ChannelType::PARTIAL_RELIABLE_REXMIT);
            reliability_parameter = uint32_t(std::max(std::get<int>(reliability_->rexmit), 0));
            break;
        }
        case SctpMessage::Reliability::Policy::TTL: {
            channel_type = uint8_t(message::ChannelType::PARTIAL_RELIABLE_TIMED);
            reliability_parameter = uint32_t(std::get<std::chrono::milliseconds>(reliability_->rexmit).count());
            break;
        }
        default: {
            channel_type = uint8_t(message::ChannelType::RELIABLE);
            reliability_parameter = 0;
            break;
        }
    }

    if (reliability_->unordered) {
        channel_type |= 0x80;
    }

    const size_t len = sizeof(message::Open) + label_.size() + protocol_.size();
    std::vector<uint8_t> buffer(len, 0);
    auto &open_msg_fixed_part = *reinterpret_cast<message::Open*>(buffer.data());
    open_msg_fixed_part.type = uint8_t(message::Type::OPEN);
    open_msg_fixed_part.channel_type = channel_type;
    open_msg_fixed_part.priority = htons(0);
    open_msg_fixed_part.reliability_parameter = reliability_parameter;
    open_msg_fixed_part.label_length = htons(uint16_t(label_.size()));
    open_msg_fixed_part.protocol_length = htons(uint16_t(protocol_.size()));

    auto open_msg_variable_part = reinterpret_cast<char*>(buffer.data() + sizeof(open_msg_fixed_part));
    std::copy(label_.begin(), label_.end(), open_msg_variable_part);
    std::copy(protocol_.begin(), protocol_.end(), open_msg_variable_part + label_.size());

    transport->Send(SctpMessage::Create(buffer.data(), buffer.size(), SctpMessage::Type::CONTROL, stream_id_, reliability_));
}

void DataChannel::SendAckMessage() const {
    auto transport = sctp_transport_.lock();
    if (!transport) {
        throw std::logic_error("DataChannel has no transport");
    }
    std::vector<uint8_t> buffer(sizeof(message::Ack), 0);
    auto &ack_msg = *reinterpret_cast<message::Ack *>(buffer.data());
    ack_msg.type = uint8_t(message::Type::ACK);

    transport->Send(SctpMessage::Create(buffer.data(), buffer.size(), SctpMessage::Type::CONTROL, stream_id_, reliability_));
}

void DataChannel::SendCloseMessage() const {
    auto transport = sctp_transport_.lock();
    if (!transport) {
        throw std::logic_error("DataChannel has no transport");
    }
    std::vector<uint8_t> buffer(sizeof(message::Close), 0);
    auto &ack_msg = *reinterpret_cast<message::Close *>(buffer.data());
    ack_msg.type = uint8_t(message::Type::CLOSE);

    transport->Send(SctpMessage::Create(buffer.data(), buffer.size(), SctpMessage::Type::CONTROL, stream_id_, reliability_));
}

void DataChannel::Send(const std::string text) {
    auto transport = sctp_transport_.lock();
    if (!transport) {
        throw std::logic_error("DataChannel has no transport");
    }
    transport->Send(SctpMessage::Create(text.c_str(), text.length(), SctpMessage::Type::STRING, stream_id_, reliability_));
}
    
} // namespace naivertc
