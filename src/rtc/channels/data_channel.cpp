#include "rtc/channels/data_channel.hpp"

#include <plog/Log.h>

namespace naivertc {

// Implement of DataChannel::Init
DataChannel::Init::Init(const std::string label_, 
                        const std::string protocol_, 
                        std::optional<StreamId> stream_id_, 
                        bool unordered_,
                        bool negotiated_) 
    : label(std::move(label_)),
      protocol(std::move(protocol_)),
      stream_id(stream_id_),
      unordered(unordered_),
      negotiated(negotiated_) {}
    
// Implement of DataChannel
std::shared_ptr<DataChannel> DataChannel::RemoteDataChannel(StreamId stream_id,
                                                            bool negotiated,
                                                            std::weak_ptr<SctpTransport> sctp_transport) {
    return std::shared_ptr<DataChannel>(new DataChannel(stream_id, negotiated, sctp_transport));
}

DataChannel::DataChannel(const std::string label, 
                         const std::string protocol, 
                         const StreamId stream_id, 
                         bool unordered,
                         bool negotiated) 
    : label_(std::move(label)),
      protocol_(std::move(protocol)),
      stream_id_(stream_id),
      negotiated_(negotiated),
      reliability_(std::make_shared<SctpMessage::Reliability>()) {
    reliability_->unordered = unordered;
}

DataChannel::DataChannel(StreamId stream_id,
                         bool negotiated,
                         std::weak_ptr<SctpTransport> sctp_transport) 
    : DataChannel("","", stream_id, false, negotiated) {
    sctp_transport_ = std::move(sctp_transport);
}

DataChannel::~DataChannel() { 
    Close(); 
}

StreamId DataChannel::stream_id() const {
    return stream_id_;
}

const std::string DataChannel::label() const {
    return label_;
}

const std::string DataChannel::protocol() const {
    return protocol_;
}

bool DataChannel::is_opened() const {
    return is_opened_;
}

void DataChannel::HintStreamId(sdp::Role role) {
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

void DataChannel::Open(std::weak_ptr<SctpTransport> sctp_transport) {
    if (is_opened_) {
        PLOG_VERBOSE << "DataChannel did open already.";
        return;
    }
    PLOG_VERBOSE << __FUNCTION__;
    sctp_transport_ = std::move(sctp_transport);
    negotiated_ ? TriggerOpen() : SendOpenMessage();
}

void DataChannel::Close() {
    if (!is_opened_) {
        PLOG_VERBOSE << "DataChannel did close already.";
        return;
    }
    PLOG_VERBOSE << __FUNCTION__;
    auto transport = sctp_transport_.lock();
    if (!transport) {
        PLOG_WARNING << "DataChannel has no transport";
        return;
    }
    is_opened_ = false;
    transport->ShutdownStream(stream_id_);
}

void DataChannel::RemoteClose() {
    PLOG_VERBOSE << __FUNCTION__;
    TriggerClose();
}

// Callback
void DataChannel::OnOpened(OpenedCallback callback) {
    opened_callback_ = callback;
}

void DataChannel::OnClosed(ClosedCallback callback) {
    closed_callback_ = callback;
}

void DataChannel::OnBinaryMessageReceivedCallback(BinaryMessageReceivedCallback callback) {
    binary_message_received_callback_ = callback;
}

void DataChannel::OnTextMessageReceivedCallback(TextMessageReceivedCallback callback) {
    text_message_received_callback_ = callback;
}

void DataChannel::OnBufferedAmountChanged(BufferedAmountChangedCallback callback) {
    buffered_amount_changed_callback_ = callback;
}

void DataChannel::TriggerOpen() {
    if (is_opened_) {
        return;
    }
    is_opened_ = true;
    if (opened_callback_) {
        opened_callback_();
    }
}

void DataChannel::TriggerClose() {
    if (!is_opened_) {
        return;
    }
    is_opened_ = false;
    if (closed_callback_) {
        closed_callback_();
    }
}

void DataChannel::TriggerAvailable(size_t count) {
    PLOG_VERBOSE << __FUNCTION__ << " => count: " << count;
    if (!is_opened_) {
        return;
    }
    ProcessPendingMessages();
}

void DataChannel::TriggerBufferedAmount(size_t amount) {
    PLOG_VERBOSE << __FUNCTION__ << " => amount: " << amount;
    if (buffered_amount_changed_callback_) {
        buffered_amount_changed_callback_(amount);
    }
}

} // namespace naivertc