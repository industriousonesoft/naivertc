#include "rtc/transports/transport.hpp"

#include <plog/Log.h>

#include <future>

namespace naivertc {

Transport::Transport(Transport* lower, TaskQueue* task_queue) 
    : lower_(lower),
      task_queue_(task_queue),
      is_stoped_(true),
      state_(State::DISCONNECTED) {
    assert(task_queue_ != nullptr);
}

Transport::~Transport() = default;

bool Transport::is_stoped() const {
    return task_queue_->Sync<bool>([this](){
        return is_stoped_;
    });
}

Transport::State Transport::state() const {
    return task_queue_->Sync<Transport::State>([this](){
        return state_;
    });
}

void Transport::OnStateChanged(StateChangedCallback callback) {
    task_queue_->Async([this, callback=std::move(callback)](){
        state_changed_callback_ = std::move(callback);
    });
}

// Protected methods
void Transport::UpdateState(State state) {
    RTC_RUN_ON(task_queue_);
    if (state_ == state) {
        return;
    }
    state_ = state;
    if (state_changed_callback_) {
        state_changed_callback_(state);
    }
}

int Transport::ForwardOutgoingPacket(CopyOnWriteBuffer packet, PacketOptions options) {
    RTC_RUN_ON(task_queue_);
    try {
        if (lower_) {
            return lower_->Send(std::move(packet), std::move(options));
        } else {
            return -1;
        }
    }catch (std::exception& e) {
        PLOG_WARNING << "Failed to forward outgoing packet: " << e.what();
        return -1;
    }
}

void Transport::ForwardIncomingPacket(CopyOnWriteBuffer packet) {
    RTC_RUN_ON(task_queue_);
    try {
        if (packet_recv_callback_) {
            packet_recv_callback_(std::move(packet));
        }
    } catch (std::exception& e) {
        PLOG_WARNING << "Failed to forward incoming packet: " << e.what();
    }
}

void Transport::RegisterIncoming() {
    RTC_RUN_ON(task_queue_);
    if (lower_) {
        PLOG_VERBOSE << "Registering incoming callback";
        lower_->packet_recv_callback_ = std::bind(&Transport::Incoming, this, std::placeholders::_1);
    }
}

void Transport::DeregisterIncoming() {
    RTC_RUN_ON(task_queue_);
    if (lower_) {
        lower_->packet_recv_callback_ = nullptr;
        PLOG_VERBOSE << "Deregistered incoming callback";
    }
}

} // namespace naivertc