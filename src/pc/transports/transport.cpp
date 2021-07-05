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
    if (!send_queue_.is_in_current_queue()) {
        send_queue_.Post([this](){
            this->Start();
        });
        return;
    }
    is_stoped_ = false;
    if (lower_) {
        lower_->OnPacketReceived(std::bind(&Transport::Incoming, this, std::placeholders::_1));
    }
    if (callback) {
        callback(std::nullopt);
    }
}

void Transport::Stop(Transport::StopedCallback callback) {
    if (!send_queue_.is_in_current_queue()) {
        send_queue_.Post([this](){
            this->Stop();
        });
        return;
    }
    is_stoped_ = true;
    if (lower_) {
        lower_->OnPacketReceived(nullptr);
    }
    if (callback) {
        callback(std::nullopt);
    }
}

bool Transport::is_stoped() const {
    if (send_queue_.is_in_current_queue()) {
        return is_stoped_;
    }
    std::promise<bool> promise;
    auto future = promise.get_future();
    send_queue_.Post([this, &promise](){
        promise.set_value(is_stoped_);
    });
    return future.get();
}

Transport::State Transport::state() const {
    if (recv_queue_.is_in_current_queue()) {
        return state_;
    }
    std::promise<State> promise;
    auto future = promise.get_future();
    recv_queue_.Post([this, &promise](){
        promise.set_value(state_);
    });
    return future.get();
}

void Transport::UpdateState(State state) {
    if (!recv_queue_.is_in_current_queue()) {
        recv_queue_.Post([this, state](){
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
    if (!recv_queue_.is_in_current_queue()) {
        recv_queue_.Post([this, packet](){
            this->HandleIncomingPacket(packet);
        });
        return;
    }
    try {
        if (this->packet_recv_callback_) {
            this->packet_recv_callback_(packet);
        }
    } catch (std::exception& exp) {
        PLOG_WARNING << exp.what();
    }
}

void Transport::OnPacketReceived(PacketReceivedCallback callback) {
    recv_queue_.Post([this, callback](){
        this->packet_recv_callback_ = std::move(callback);
    });
}

void Transport::Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) {
    if (!send_queue_.is_in_current_queue()) {
        send_queue_.Post([this, packet, callback](){
            this->Send(packet, callback);
        });
        return;
    }
    Outgoing(packet, callback);
}

void Transport::Incoming(std::shared_ptr<Packet> in_packet) {
    if (!recv_queue_.is_in_current_queue()) {
        recv_queue_.Post([this, in_packet](){
            this->Incoming(in_packet);
        });
        return;
    }
    HandleIncomingPacket(in_packet);
}

void Transport::Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback) {
    if (!send_queue_.is_in_current_queue()) {
        send_queue_.Post([this, out_packet, callback](){
            this->Outgoing(out_packet, callback);
        });
        return;
    }
    if (lower_) {
        lower_->Send(out_packet, callback);
    }else if (callback) {
        callback(false);
    }
}

} // end of naivertc