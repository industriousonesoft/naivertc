#ifndef _RTC_TRANSPORT_H_
#define _RTC_TRANSPORT_H_

#include "base/defines.hpp"
#include "base/packet.hpp"
#include "common/task_queue.hpp"

#include <memory>
#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT Transport : public std::enable_shared_from_this<Transport> {
public:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        COMPLETED,
        FAILED
    };

    using StartedCallback = std::function<void(std::optional<const std::exception>)>;
    using StopedCallback = std::function<void(std::optional<const std::exception>)>;
    using PacketSentCallback = std::function<void(size_t sent_size)>;

    using StateChangedCallback = std::function<void(State state)>;
    using PacketReceivedCallback = std::function<void(std::shared_ptr<Packet> in_packet)>;
public:
    Transport(std::shared_ptr<Transport> lower = nullptr);
    virtual ~Transport();

    bool is_stoped() const;
    State state() const;

    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    
    virtual void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) = 0;
    virtual int Send(std::shared_ptr<Packet> packet) = 0;

    void OnStateChanged(StateChangedCallback callback);    
    void OnPacketReceived(PacketReceivedCallback callback);
    
protected:
    virtual void Incoming(std::shared_ptr<Packet> in_packet) = 0;
    virtual int Outgoing(std::shared_ptr<Packet> in_packet) = 0;
  
    void UpdateState(State state);

    void RegisterIncoming();
    void DeregisterIncoming();

    void ForwardIncomingPacket(std::shared_ptr<Packet> packet);
    void ForwardOutgoingPacket(std::shared_ptr<Packet> out_packet, PacketSentCallback callback);
    int ForwardOutgoingPacket(std::shared_ptr<Packet> out_packet);

protected:
    std::shared_ptr<Transport> lower_;

    TaskQueue task_queue_;
    
    bool is_stoped_;
    State state_;

    PacketReceivedCallback packet_recv_callback_ = nullptr;
    StateChangedCallback state_changed_callback_ = nullptr;

};

}

#endif