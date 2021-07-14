#include "pc/transports/transport.hpp"

#include <plog/Log.h>

#include <future>

namespace naivertc {

Transport::Transport(std::shared_ptr<Transport> lower) 
    : lower_(lower), 
    packet_recv_callback_(nullptr), 
    is_stoped_(false),
    state_(State::DISCONNECTED) {
    
}

Transport::~Transport() {

}

void Transport::Start(Transport::StartedCallback callback) {
    if (!task_queue_.is_in_current_queue()) {
        task_queue_.Post([this](){
            this->Start();
        });
        return;
    }
    try {
        if (lower_) {
            lower_->OnPacketReceived(std::bind(&Transport::Incoming, this, std::placeholders::_1));
        }
        is_stoped_ = false;
        if (callback) {
            callback(std::nullopt);
        }
    }catch (const std::exception& exp) {
        if (callback) {
            callback(exp);
        }
    }
}

void Transport::Stop(Transport::StopedCallback callback) {
    if (!task_queue_.is_in_current_queue()) {
        task_queue_.Post([this](){
            this->Stop();
        });
        return;
    }
    try {
        is_stoped_ = true;
        if (lower_) {
            lower_->OnPacketReceived(nullptr);
        }
        if (callback) {
            callback(std::nullopt);
        }
    }catch (const std::exception& exp) {
        if (callback) {
            callback(exp);
        }
    }
}

bool Transport::is_stoped() const {
    if (task_queue_.is_in_current_queue()) {
        return is_stoped_;
    }
    return task_queue_.SyncPost<bool>([this]() -> bool {
        return is_stoped_;
    });
}

Transport::State Transport::state() const {
    if (task_queue_.is_in_current_queue()) {
        return state_;
    }
    return task_queue_.SyncPost<Transport::State>([this]() -> Transport::State {
        return state_;
    });
}

void Transport::UpdateState(State state) {
    if (!task_queue_.is_in_current_queue()) {
        task_queue_.Post([this, state](){
            this->UpdateState(state);
        });
        return;
    }
    if (state_ == state) 
        return;
    state_ = state;
    SignalStateChanged(state);
}

void Transport::HandleIncomingPacket(std::shared_ptr<Packet> packet) {
    if (!task_queue_.is_in_current_queue()) {
        task_queue_.Post([this, packet = std::move(packet)](){
            this->HandleIncomingPacket(std::move(packet));
        });
        return;
    }
    try {
        if (this->packet_recv_callback_) {
            this->packet_recv_callback_(std::move(packet));
        }
    } catch (std::exception& exp) {
        PLOG_WARNING << exp.what();
    }
}

void Transport::OnPacketReceived(PacketReceivedCallback callback) {
    task_queue_.Post([this, callback](){
        this->packet_recv_callback_ = std::move(callback);
    });
}

void Transport::Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) {
    if (!task_queue_.is_in_current_queue()) {
        task_queue_.Post([this, packet = std::move(packet), callback](){
            this->Send(std::move(packet), callback);
        });
        return;
    }
    Outgoing(std::move(packet), callback);
}

void Transport::Incoming(std::shared_ptr<Packet> in_packet) {
    if (!task_queue_.is_in_current_queue()) {
        task_queue_.Post([this, in_packet = std::move(in_packet)](){
            this->Incoming(std::move(in_packet));
        });
        return;
    }
    HandleIncomingPacket(std::move(in_packet));
}

void Transport::Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback) {
    if (!task_queue_.is_in_current_queue()) {
        task_queue_.Post([this, out_packet = std::move(out_packet), callback](){
            this->Outgoing(std::move(out_packet), callback);
        });
        return;
    }
    if (lower_) {
        try {
            lower_->Send(std::move(out_packet), callback);
        }catch (const std::exception& exp) {
            PLOG_WARNING << exp.what();
            if (callback) {
                callback(false);
            }
        }
    }else if (callback) {
        callback(false);
    }
}

} // namespace naivertc