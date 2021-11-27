#ifndef _RTC_TRANSPORTS_TRANSPORT_H_
#define _RTC_TRANSPORTS_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/base/packet_options.hpp"

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
    
public:
    Transport(std::weak_ptr<Transport> lower);

    bool is_stoped() const;
    State state() const;

    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    
    virtual int Send(CopyOnWriteBuffer packet, const PacketOptions& options) = 0;

    using StateChangedCallback = std::function<void(State state)>;
    void OnStateChanged(StateChangedCallback callback);    

protected:
    virtual ~Transport();
    
    virtual void Incoming(CopyOnWriteBuffer packet) = 0;
    virtual int Outgoing(CopyOnWriteBuffer packet, const PacketOptions& options) = 0;
  
    void UpdateState(State state);

    void RegisterIncoming();
    void DeregisterIncoming();

    void ForwardIncomingPacket(CopyOnWriteBuffer packet);
    int ForwardOutgoingPacket(CopyOnWriteBuffer packet, const PacketOptions& options);

protected:
    SequenceChecker sequence_checker_;
    std::weak_ptr<Transport> lower_;

    bool is_stoped_;
    State state_;

    using PacketReceivedCallback = std::function<void(CopyOnWriteBuffer packet)>;
    PacketReceivedCallback packet_recv_callback_ = nullptr;
    StateChangedCallback state_changed_callback_ = nullptr;
};

}

#endif