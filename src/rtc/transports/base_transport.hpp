#ifndef _RTC_TRANSPORTS_TRANSPORT_H_
#define _RTC_TRANSPORTS_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/base/packet_options.hpp"

#include <memory>
#include <functional>

namespace naivertc {

class BaseTransport {
public:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        COMPLETED,
        FAILED
    };
    
public:
    BaseTransport(BaseTransport* lower);

    bool is_stoped() const;
    State state() const;

    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    
    virtual int Send(CopyOnWriteBuffer packet, PacketOptions options) = 0;

    using StateChangedCallback = std::function<void(State state)>;
    void OnStateChanged(StateChangedCallback callback);    

protected:
    virtual ~BaseTransport();
    
    virtual void Incoming(CopyOnWriteBuffer packet) = 0;
    virtual int Outgoing(CopyOnWriteBuffer packet, PacketOptions options) = 0;
  
    void UpdateState(State state);

    void RegisterIncoming();
    void DeregisterIncoming();

    void ForwardIncomingPacket(CopyOnWriteBuffer packet);
    int ForwardOutgoingPacket(CopyOnWriteBuffer packet, PacketOptions options);

protected:
    SequenceChecker sequence_checker_;
    TaskQueueImpl* const attached_queue_;
    BaseTransport* const lower_;

    bool is_stoped_;
    State state_;

    using PacketReceivedCallback = std::function<void(CopyOnWriteBuffer packet)>;
    PacketReceivedCallback packet_recv_callback_ = nullptr;
    StateChangedCallback state_changed_callback_ = nullptr;
};

}

#endif