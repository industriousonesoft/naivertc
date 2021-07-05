#ifndef _PC_TRANSPORT_H_
#define _PC_TRANSPORT_H_

#include "base/defines.hpp"
#include "base/packet.hpp"
#include "common/task_queue.hpp"

#include <sigslot.h>

#include <memory>
#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT Transport : public sigslot::has_slots<>, public std::enable_shared_from_this<Transport> {
public:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        COMPLETED,
        FAILED
    };
public:
    Transport(std::shared_ptr<Transport> lower = nullptr);
    virtual ~Transport();

    // SHOULD connect slots after created instance immediately to avoiding racing.
    sigslot::signal1<State> SignalStateChanged;

    bool is_stoped() const;
    State state() const;

    using StartedCallback = std::function<void(std::optional<const std::exception>)>;
    using StopedCallback = std::function<void(std::optional<const std::exception>)>;
    virtual void Start(StartedCallback callback = nullptr);
    virtual void Stop(StopedCallback callback = nullptr);

    using PacketReceivedCallback = std::function<void(std::shared_ptr<Packet> in_packet)>;
    void OnPacketReceived(PacketReceivedCallback callback);
    
    using PacketSentCallback = std::function<void(bool success)>;
    virtual void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback = nullptr);

protected:
    virtual void Incoming(std::shared_ptr<Packet> in_packet);
    virtual void Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback = nullptr);

    void UpdateState(State state);
    void HandleIncomingPacket(std::shared_ptr<Packet> packet);

    TaskQueue task_queue_;

private:
    std::shared_ptr<Transport> lower_;
    PacketReceivedCallback packet_recv_callback_;
    bool is_stoped_;
    State state_;

};

}

#endif