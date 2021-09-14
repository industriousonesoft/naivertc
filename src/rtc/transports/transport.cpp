#include "rtc/transports/transport.hpp"

#include <plog/Log.h>

#include <future>

namespace naivertc {

Transport::Transport(std::shared_ptr<Transport> lower, std::shared_ptr<TaskQueue> task_queue) 
    : lower_(std::move(lower)), 
    is_stoped_(true),
    state_(State::DISCONNECTED) {
    task_queue_ = task_queue ? std::move(task_queue) : std::make_shared<TaskQueue>("TransportTaskQueue");
}

Transport::~Transport() {}

bool Transport::is_stoped() const {
    return task_queue_->Sync<bool>([this]() -> bool {
        return is_stoped_;
    });
}

Transport::State Transport::state() const {
    return task_queue_->Sync<Transport::State>([this]() -> Transport::State {
        return state_;
    });
}

void Transport::OnStateChanged(StateChangedCallback callback) {
    task_queue_->Async([this, callback](){
        this->state_changed_callback_ = std::move(callback);
    });
}

void Transport::OnPacketReceived(PacketReceivedCallback callback) {
    task_queue_->Async([this, callback](){
        this->packet_recv_callback_ = std::move(callback);
    });
}

// Protected methods
void Transport::RegisterIncoming() {
    task_queue_->Async([this](){
        if (lower_) {
            PLOG_VERBOSE << "Registering incoming callback";
            lower_->OnPacketReceived(std::bind(&Transport::Incoming, this, std::placeholders::_1));
        }
    });
}

void Transport::DeregisterIncoming() {
    task_queue_->Async([this](){
        if (lower_) {
            lower_->OnPacketReceived(nullptr);
            PLOG_VERBOSE << "Deregistered incoming callback";
        }
    });
}

void Transport::UpdateState(State state) {
    task_queue_->Async([this, state](){
        if (state_ == state) 
            return;
        state_ = state;
        if (state_changed_callback_) {
            state_changed_callback_(state);
        }
    });
}

void Transport::ForwardOutgoingPacket(Packet packet, PacketSentCallback callback) {
    task_queue_->Async([this, packet = std::move(packet), callback](){
        if (lower_) {
            lower_->Send(std::move(packet), callback);
        }else {
            callback(-1);
        }
    });
}

int Transport::ForwardOutgoingPacket(Packet packet) {
    return task_queue_->Sync<int>([this, packet=std::move(packet)](){
        if (lower_) {
            return lower_->Send(std::move(packet));
        }
        return -1;
    });
}

void Transport::ForwardIncomingPacket(Packet packet) {
    task_queue_->Async([this, packet=std::move(packet)](){
        try {
            if (this->packet_recv_callback_) {
                this->packet_recv_callback_(std::move(packet));
            }
        } catch (std::exception& exp) {
            PLOG_WARNING << exp.what();
        }
    });
}

} // namespace naivertc