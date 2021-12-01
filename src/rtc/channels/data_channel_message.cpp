#include "rtc/channels/data_channel.hpp"
#include "rtc/transports/sctp_transport.hpp"

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

void DataChannel::OnIncomingMessage(SctpMessage message) {
    task_queue_.Async([this, message=std::move(message)]() mutable {
        switch (message.type()) {
            case SctpMessage::Type::CONTROL: {
                const auto& payload = message.payload();
                if (IsOpenMessage(payload)) {
                    ProcessOpenMessage(message);
                } else if (IsAckMessage(payload)) {
                    TriggerOpen();
                } else if (IsCloseMessage(payload)) {
                    // The close message will be processted in-order
                    pending_incoming_messages_.push(std::move(message));
                    ProcessPendingIncomingMessages();
                }
                break;
            }
            case SctpMessage::Type::STRING: {
                pending_incoming_messages_.push(std::move(message));
                ProcessPendingIncomingMessages();
                break;
            }
            case SctpMessage::Type::BINARY: {
                pending_incoming_messages_.push(std::move(message));
                ProcessPendingIncomingMessages();
                break;
            }
            default:
                // Ignore
                break;
        }
    });
}

void DataChannel::Send(const std::string text) {
    task_queue_.Async([this, text=std::move(text)](){
        if (transport_) {
            CopyOnWriteBuffer payload(reinterpret_cast<const uint8_t*>(text.c_str()), text.length());
            Send(SctpMessageToSend(SctpMessage::Type::STRING, stream_id_, std::move(payload), user_message_reliability_));
        } else {
            PLOG_WARNING << "The data channel is not ready to send data.";
        }
    });
}

// Protected methods
void DataChannel::ProcessOpenMessage(const SctpMessage& message) {
    if (config_.negotiated) {
        PLOG_WARNING << "The open messages for a user-negotiated DataChannel received, ignoring";
    } else {
        try {
            // Negotiating with remote data channel
            ParseOpenMessage(message.payload(), config_);
            // Send back a ACK message
            SendAckMessage();
            // Trigger open callback
            TriggerOpen();
        }catch (const std::exception& e) {
            PLOG_ERROR << "Failed to open data channel: " << e.what();
            TriggerClose();
        }
    }
}

void DataChannel::ProcessPendingIncomingMessages() {
    while(!pending_incoming_messages_.empty()) {
        const auto& message = pending_incoming_messages_.front();
        if (message.type() == SctpMessage::Type::BINARY) {
            if (binary_message_received_callback_) {
                binary_message_received_callback_(message.payload().data(), message.payload().size());
            }
        } else if (message.type() == SctpMessage::Type::STRING) {
            std::string text = std::string(message.payload().cbegin(), message.payload().cend());
            if (text_message_received_callback_) {
                text_message_received_callback_(text);
            } else {
                PLOG_INFO << "Receive text: " << text;
            }
        } else {
            // Close message from remote peer
            if (message.type() == SctpMessage::Type::CONTROL && IsCloseMessage(message.payload())) {
                RemoteClose();
            }
        }
        pending_incoming_messages_.pop();
    }
}

void DataChannel::SendOpenMessage() {
    Send(SctpMessageToSend(SctpMessage::Type::CONTROL, stream_id_, CreateOpenMessage(config_), control_message_reliability_));
}

void DataChannel::SendAckMessage() {
    Send(SctpMessageToSend(SctpMessage::Type::CONTROL, stream_id_, CreateAckMessage(), control_message_reliability_));
}

void DataChannel::SendCloseMessage() {
    Send(SctpMessageToSend(SctpMessage::Type::CONTROL, stream_id_, CreateCloseMessage(), control_message_reliability_));
}

void DataChannel::Send(SctpMessageToSend message) {
    if (FlushPendingMessages() && TrySend(message)) {
        return;
    }
    // Update bufferd amount
    UpdateBufferedAmount(ptrdiff_t(message.payload().size()));
    // Enqueue
    pending_outgoing_messages_.push(std::move(message));
}

bool DataChannel::FlushPendingMessages() {
	while (!pending_outgoing_messages_.empty()) {
		auto message = pending_outgoing_messages_.front();
		if (!TrySend(message)) {
			return false;
		}
		pending_outgoing_messages_.pop();
		UpdateBufferedAmount(-ptrdiff_t(message.payload().size()));
	}
	return true;
}

bool DataChannel::TrySend(SctpMessageToSend message) {
    if (!transport_) {
        PLOG_WARNING << "Failed to send message cause the sctp transport is not set yet.";
        return false;
    }
    return transport_->Send(std::move(message));;
}
    
} // namespace naivertc
