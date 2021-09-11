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
    auto dc = std::shared_ptr<DataChannel>(new DataChannel("","", stream_id, false, negotiated));
    dc->sctp_transport_ = std::move(sctp_transport);
    return dc;
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
      reliability_(std::make_shared<SctpMessage::Reliability>()),
      task_queue_("DataChannel." + label + "."+ std::to_string(stream_id) +".work.queue") {
    reliability_->unordered = unordered;
}

DataChannel::~DataChannel() { 
    Close(); 
}

StreamId DataChannel::stream_id() const {
    return task_queue_.Sync<StreamId>([this](){
        return stream_id_;
    });
}

const std::string DataChannel::label() const {
    return task_queue_.Sync<std::string>([this](){
        return label_;
    });
}

const std::string DataChannel::protocol() const {
    return task_queue_.Sync<std::string>([this](){
        return protocol_;
    });
}

bool DataChannel::is_opened() const {
    return task_queue_.Sync<bool>([this](){
        return is_opened_;
    });
}

void DataChannel::HintStreamId(sdp::Role role) {
    task_queue_.Async([this, role](){
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
    });
}

void DataChannel::Open(std::weak_ptr<SctpTransport> sctp_transport) {
    task_queue_.Async([this, sctp_transport=std::move(sctp_transport)](){
        if (is_opened_) {
            PLOG_VERBOSE << "DataChannel: " + std::to_string(stream_id_) + "did open already.";
            return;
        }
        PLOG_VERBOSE << __FUNCTION__;
        sctp_transport_ = std::move(sctp_transport);
        negotiated_ ? TriggerOpen() : SendOpenMessage();
    });
    
}

void DataChannel::Close() {
    task_queue_.Async([this](){
        if (!is_opened_) {
            PLOG_VERBOSE << "DataChannel:" + std::to_string(stream_id_) + " did close already.";
            return;
        }
        PLOG_VERBOSE << __FUNCTION__;
        is_opened_ = false;
        if (auto transport = sctp_transport_.lock()) {
            transport->ShutdownStream(stream_id_);
        }
    });
}

void DataChannel::RemoteClose() {
    task_queue_.Async([this](){
        PLOG_VERBOSE << __FUNCTION__;
        TriggerClose();
    });
}

// Callback
void DataChannel::OnOpened(OpenedCallback callback) {
    task_queue_.Async([this, callback=std::move(callback)](){
        opened_callback_ = callback;
    });
}

void DataChannel::OnClosed(ClosedCallback callback) {
    task_queue_.Async([this, callback=std::move(callback)](){
        closed_callback_ = callback;
    });
}

void DataChannel::OnBinaryMessageReceivedCallback(BinaryMessageReceivedCallback callback) {
    task_queue_.Async([this, callback=std::move(callback)](){
        binary_message_received_callback_ = callback;
        // TODO: Flush pending binary message 
    });
}

void DataChannel::OnTextMessageReceivedCallback(TextMessageReceivedCallback callback) {
    task_queue_.Async([this, callback=std::move(callback)](){
        text_message_received_callback_ = callback;
        // TODO: Flush pending text message 
    });
}

void DataChannel::OnBufferedAmountChanged(BufferedAmountChangedCallback callback) {
    task_queue_.Async([this, callback=std::move(callback)](){
        buffered_amount_changed_callback_ = callback;
    });
}

// Protected methods
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

void DataChannel::TriggerBufferedAmount(size_t amount) {
    PLOG_VERBOSE << __FUNCTION__ << " => amount: " << amount;
    if (buffered_amount_changed_callback_) {
        buffered_amount_changed_callback_(amount);
    }
}

} // namespace naivertc