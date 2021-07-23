#include "rtc/transports/transport.hpp"

#include <plog/Log.h>

#include <future>

namespace naivertc {

Transport::Transport(std::shared_ptr<Transport> lower) 
    : lower_(lower), 
    is_stoped_(true),
    state_(State::DISCONNECTED) {}

Transport::~Transport() {}

bool Transport::is_stoped() const {
    return task_queue_.SyncPost<bool>([this]() -> bool {
        return is_stoped_;
    });
}

Transport::State Transport::state() const {
    return task_queue_.SyncPost<Transport::State>([this]() -> Transport::State {
        return state_;
    });
}

void Transport::OnStateChanged(StateChangedCallback callback) {
    task_queue_.Post([this, callback](){
        this->state_changed_callback_ = std::move(callback);
    });
}

void Transport::OnPacketReceived(PacketReceivedCallback callback) {
    task_queue_.Post([this, callback](){
        this->packet_recv_callback_ = std::move(callback);
    });
}

// Protected methods
void Transport::RegisterIncoming() {
    if (lower_) {
        PLOG_VERBOSE << "Registering incoming callback";
        lower_->OnPacketReceived(std::bind(&Transport::Incoming, this, std::placeholders::_1));
    }
}

void Transport::DeregisterIncoming() {
    if (lower_) {
        lower_->OnPacketReceived(nullptr);
        PLOG_VERBOSE << "Deregistered incoming callback";
    }
}

void Transport::UpdateState(State state) {
    if (state_ == state) 
        return;
    state_ = state;
    if (state_changed_callback_) {
        state_changed_callback_(state);
    }
}

void Transport::ForwardOutgoingPacket(std::shared_ptr<Packet> out_packet, PacketSentCallback callback) {
    task_queue_.Post([this, out_packet = std::move(out_packet), callback](){
        if (lower_) {
            lower_->Send(std::move(out_packet), callback);
        }else {
            callback(-1);
        }
    });
}

int Transport::ForwardOutgoingPacket(std::shared_ptr<Packet> out_packet) {
    if (lower_) {
        return lower_->Send(std::move(out_packet));
    }
    return -1;
}

void Transport::ForwardIncomingPacket(std::shared_ptr<Packet> packet) {
    try {
        if (this->packet_recv_callback_) {
            this->packet_recv_callback_(std::move(packet));
        }
    } catch (std::exception& exp) {
        PLOG_WARNING << exp.what();
    }
}

} // namespace naivertc