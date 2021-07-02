#include "pc/transports/transport.hpp"

#include <plog/Log.h>

namespace naivertc {

Transport::Transport(std::shared_ptr<Transport> lower) 
    : lower_(lower), 
    packet_recv_callback_(nullptr) {}

Transport::~Transport() {

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
    try {
        if (this->packet_recv_callback_) {
            this->packet_recv_callback_(in_packet);
        }
    } catch (std::exception& exp) {
        PLOG_WARNING << exp.what();
    }
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

}