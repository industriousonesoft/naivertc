#include "rtc/channels/data_channel.hpp"
#include "common/utils.hpp"

namespace naivertc {

// Implement of DataChannel::Init
DataChannel::Init::Init(const std::string label_, const std::string protocol_, std::optional<StreamId> stream_id_, bool unordered_) 
    : label(std::move(label_)),
    protocol(std::move(protocol_)),
    stream_id(stream_id_),
    unordered(unordered_) {}
    
// Implement of DataChannel
DataChannel::DataChannel(const std::string label, const std::string protocol, const StreamId stream_id, bool unordered) 
    : label_(std::move(label)),
    protocol_(std::move(protocol)),
    stream_id_(stream_id) {
    reliability_ = std::make_shared<SctpMessage::Reliability>();
    reliability_->unordered = unordered;
}

DataChannel::DataChannel(StreamId stream_id) 
    : DataChannel("","", stream_id, false) {}

DataChannel::~DataChannel() {}

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

void DataChannel::Open() {
    SendOpenMessage();
}

void DataChannel::Close() {
    auto transport = sctp_transport_.lock();
    if (!transport) {
        throw std::runtime_error("DataChannel has no transport");
    }
    is_opened_ = false;
    transport->CloseStream(stream_id_);
}

void DataChannel::RemoteClose() {
    if (!is_opened_) {
        return;
    }
    TriggerClose();
    is_opened_ = false;
}

void DataChannel::AttachTo(std::weak_ptr<SctpTransport> sctp_transport) {
    sctp_transport_ = sctp_transport;
}

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
        opened_callback_(stream_id_);
    }
}

void DataChannel::TriggerClose() {
    if (closed_callback_) {
        closed_callback_(stream_id_);
    }
}

void DataChannel::TriggerAvailable(size_t count) {
    if (!is_opened_) {
        return;
    }
    ProcessPendingMessages();
}

void DataChannel::TriggerBufferedAmount(size_t amount) {
    if (buffered_amount_changed_callback_) {
        buffered_amount_changed_callback_(amount);
    }
}

} // namespace naivertc