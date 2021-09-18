#ifndef _RTC_TRANSPORTS_SCTP_PACKET_H_
#define _RTC_TRANSPORTS_SCTP_PACKET_H_

#include "base/defines.hpp"
#include "rtc/base/internals.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"

#include <chrono>
#include <variant>
#include <iostream>
#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT SctpMessage {
public:
    enum class Type {
        BINARY,
        STRING,
        CONTROL
    };
public:
    SctpMessage(Type type, 
                uint16_t stream_id,
                CopyOnWriteBuffer payload);
    virtual ~SctpMessage();

    Type type() const { return type_; }
    uint16_t stream_id() const { return stream_id_; }
    const CopyOnWriteBuffer& payload() const { return payload_; }

protected:
    const Type type_;
    const uint16_t stream_id_;
    const CopyOnWriteBuffer payload_;
};

// SctpMessageToSend
class RTC_CPP_EXPORT SctpMessageToSend : public SctpMessage {
public:
    // The reliability may change from message to message, even wihin a single channel.
    // For example, control message may be sent reliably and in-order, even if the data
    // channel is configured for unreliable delivery.
    struct Reliability {
        Reliability() = default;
        Reliability(const Reliability&) = default;

        // Whether to deliver the message in order with respect to other ordered
        // messages with the same channel_id.
        bool ordered = false;

        // If set, the maximum number of times this message may be
        // retransmitted by the transport before it is dropped.
        // Setting this value to zero disables retransmission.
        // Valid values are in the range [0-UINT16_MAX].
        // `max_rtx_count` and `max_rtx_ms` may not be set simultaneously.
        std::optional<uint16_t> max_rtx_count = std::nullopt;

        // If set, the maximum number of milliseconds for which the transport
        // may retransmit this message before it is dropped.
        // Setting this value to zero disables retransmission.
        // Valid values are in the range [0-UINT16_MAX].
        // `max_rtx_count` and `max_rtx_ms` may not be set simultaneously.
        std::optional<uint16_t> max_rtx_ms = std::nullopt;
    };
public:
    SctpMessageToSend(Type type, 
                      uint16_t stream_id,
                      CopyOnWriteBuffer payload,
                      const Reliability& reliability);
    ~SctpMessageToSend();

    const Reliability& reliability() const { return reliability_; }

private:
    friend class SctpTransport;
        
    size_t available_payload_size() const;
    const uint8_t* available_payload_data() const;
    void Advance(size_t increment);

private:
    const Reliability reliability_;
    size_t offset_ = 0;
};

RTC_CPP_EXPORT std::ostream& operator<<(std::ostream &out, naivertc::SctpMessage::Type type);

} // namespace naivertc

#endif