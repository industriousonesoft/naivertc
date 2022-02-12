#include "rtc/data_channel/data_channel.hpp"
#include "rtc/transports/sctp_transport.hpp"

#include <plog/Log.h>

namespace naivertc {

DataChannel::Init::Init(std::string label) 
    : label(std::move(label)) {}

// Implement of DataChannel
std::shared_ptr<DataChannel> DataChannel::RemoteDataChannel(uint16_t stream_id,
                                                            bool negotiated,
                                                            std::weak_ptr<DataTransport> transport) {
    Init init("");
    init.negotiated = negotiated;
    auto dc = std::shared_ptr<DataChannel>(new DataChannel(init, stream_id));
    dc->send_transport_ = std::move(transport);
    return dc;
}

DataChannel::DataChannel(const Init& init_config, uint16_t stream_id) 
    : config_(init_config),
      stream_id_(stream_id),
      task_queue_(std::make_unique<TaskQueue>("DataChannel." + config_.label + "."+ std::to_string(stream_id) +".task.queue")) {
    // User message reliability
    user_message_reliability_.ordered = config_.ordered;
    user_message_reliability_.max_rtx_count = config_.max_rtx_count;
    user_message_reliability_.max_rtx_ms = config_.max_rtx_ms;
    // Control message reliablity
    // The control message may be sent reliably and in-order
    control_message_reliability_.ordered = true;
    if (config_.max_rtx_count.has_value()) {
        control_message_reliability_.max_rtx_count = config_.max_rtx_count.value();
    } else if (config_.max_rtx_ms.has_value()) {
        control_message_reliability_.max_rtx_ms = config_.max_rtx_ms.value();
    } else {
        // TODO: How to set a suitable reliability for control message?
        control_message_reliability_.max_rtx_count = 5;
        control_message_reliability_.max_rtx_ms = std::nullopt;
    }
    
}

DataChannel::~DataChannel() { 
    Close(); 
}

uint16_t DataChannel::stream_id() const {
    return task_queue_->Invoke<uint16_t>([this](){
        return stream_id_;
    });
}

const std::string DataChannel::label() const {
    return task_queue_->Invoke<std::string>([this](){
        return config_.label;
    });
}

const std::string DataChannel::protocol() const {
    return task_queue_->Invoke<std::string>([this](){
        return config_.protocol;
    });
}

bool DataChannel::is_opened() const {
    return task_queue_->Invoke<bool>([this](){
        return is_opened_;
    });
}

void DataChannel::HintStreamId(sdp::Role role) {
    task_queue_->Post([this, role](){
        // FRC 8832: The peer that initiates opening a data channel selects a stream identifier for 
        // which the corresponding incoming and outgoing streams are unused. If the side is acting as the DTLS client,
        // it MUST choose an even stream identifier, if the side is acting as the DTLS server, it MUST choose an odd one.
        // See https://tools.ietf.org/html/rfc8832#section-6
        if (role == sdp::Role::ACTIVE) {
            if (stream_id_ % 2 == 1) {
                stream_id_ -= 1;
            }
        } else if (role == sdp::Role::PASSIVE) {
            if (stream_id_ % 2 == 0) {
                stream_id_ += 1;
            }
        } else {
            // This is ok
        }
    });
}

void DataChannel::Open(std::weak_ptr<DataTransport> transport) {
    task_queue_->Post([this, transport](){
        if (is_opened_) {
            PLOG_VERBOSE << "DataChannel: " + std::to_string(stream_id_) + "did open already.";
            return;
        }
        PLOG_VERBOSE << __FUNCTION__;
        send_transport_ = std::move(transport);
        config_.negotiated ? TriggerOpen() : SendOpenMessage();
    });
    
}

void DataChannel::Close(bool by_remote) {
    task_queue_->Post([this, by_remote](){
        if (!is_opened_) {
            return;
        }
        if (!by_remote) {
            CloseStream();
        }
        Reset();
        TriggerClose();
        PLOG_VERBOSE << "DataChannel[stream id=" + std::to_string(stream_id_) + "] did close.";
    });
}

// Callback
void DataChannel::OnOpened(OpenedCallback callback) {
    task_queue_->Post([this, callback=std::move(callback)](){
        opened_callback_ = callback;
    });
}

void DataChannel::OnClosed(ClosedCallback callback) {
    task_queue_->Post([this, callback=std::move(callback)](){
        closed_callback_ = callback;
    });
}

void DataChannel::OnMessageReceived(BinaryMessageReceivedCallback callback) {
    task_queue_->Post([this, callback=std::move(callback)](){
        binary_message_received_callback_ = callback;
        // TODO: Flush pending binary message 
    });
}

void DataChannel::OnMessageReceived(TextMessageReceivedCallback callback) {
    task_queue_->Post([this, callback=std::move(callback)](){
        text_message_received_callback_ = callback;
        // TODO: Flush pending text message 
    });
}

void DataChannel::OnBufferedAmountChanged(BufferedAmountChangedCallback callback) {
    task_queue_->Post([this, callback=std::move(callback)](){
        buffered_amount_changed_callback_ = callback;
    });
}

void DataChannel::OnReadyToSend() {
    task_queue_->Post([this](){
        FlushPendingMessages();
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

void DataChannel::UpdateBufferedAmount(ptrdiff_t delta) {
	size_t amount = size_t(std::max(buffered_amount_ + delta, ptrdiff_t(0)));
	if (buffered_amount_changed_callback_) {
		buffered_amount_changed_callback_(amount);
	}
}

void DataChannel::Reset() {
    std::queue<SctpMessageToSend>().swap(pending_outgoing_messages_);
	std::queue<SctpMessage>().swap(pending_incoming_messages_);
    buffered_amount_ = 0;
    send_transport_.reset();
}

void DataChannel::CloseStream() {
    if (auto transport = send_transport_.lock()) {
        transport->Send(SctpMessageToSend(SctpMessage::Type::CLOSE, stream_id_, {}, control_message_reliability_));
    }
}

} // namespace naivertc