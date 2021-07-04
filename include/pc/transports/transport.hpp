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

    using StartedCallback = std::function<void(std::optional<const std::exception>)>;
    virtual void Start(StartedCallback callback = nullptr);
    using StopedCallback = std::function<void(std::optional<const std::exception>)>;
    virtual void Stop(StopedCallback callback = nullptr);

    using PacketReceivedCallback = std::function<void(std::shared_ptr<Packet> in_packet)>;
    void OnPacketReceived(PacketReceivedCallback callback);
    
    using PacketSentCallback = std::function<void(bool success)>;
    virtual void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback = nullptr);

protected:
    virtual void Incoming(std::shared_ptr<Packet> in_packet);
    virtual void Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback = nullptr);

    State state() const;
    void UpdateState(State state); 

    TaskQueue send_queue_;
    TaskQueue recv_queue_;

private:
    std::shared_ptr<Transport> lower_;
    PacketReceivedCallback packet_recv_callback_;
    bool is_stoped_;
    State state_;

};

}

#endif