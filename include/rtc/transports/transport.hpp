#ifndef _RTC_TRANSPORTS_TRANSPORT_H_
#define _RTC_TRANSPORTS_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/base/packet.hpp"
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
    using PacketSentCallback = std::function<void(int sent_size)>;

    using StateChangedCallback = std::function<void(State state)>;
    using PacketReceivedCallback = std::function<void(Packet in_packet)>;
public:
    Transport(std::shared_ptr<Transport> lower = nullptr, std::shared_ptr<TaskQueue> task_queue = nullptr);
    virtual ~Transport();

    bool is_stoped() const;
    State state() const;

    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    
    virtual void Send(Packet packet, PacketSentCallback callback) = 0;
    virtual int Send(Packet packet) = 0;

    void OnStateChanged(StateChangedCallback callback);    
    void OnPacketReceived(PacketReceivedCallback callback);
    
protected:
    virtual void Incoming(Packet packet) = 0;
    virtual int Outgoing(Packet packet) = 0;
  
    void UpdateState(State state);

    void RegisterIncoming();
    void DeregisterIncoming();

    void ForwardIncomingPacket(Packet packet);
    void ForwardOutgoingPacket(Packet packet, PacketSentCallback callback);
    int ForwardOutgoingPacket(Packet packet);

protected:
    std::shared_ptr<Transport> lower_;
    bool is_stoped_;
    State state_;

    std::shared_ptr<TaskQueue> task_queue_;

    PacketReceivedCallback packet_recv_callback_ = nullptr;
    StateChangedCallback state_changed_callback_ = nullptr;

};

}

#endif