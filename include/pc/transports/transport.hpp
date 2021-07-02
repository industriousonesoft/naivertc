#ifndef _PC_TRANSPORT_H_
#define _PC_TRANSPORT_H_

#include "base/defines.hpp"
#include "base/packet.hpp"
#include "common/task_queue.hpp"

#include <sigslot.h>

#include <memory>
#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT Transport: sigslot::has_slots<> {
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

    sigslot::signal1<State> SignalStateChanged;

    using PacketReceivedCallback = std::function<void(std::shared_ptr<Packet> in_packet)>;
    void OnPacketReceived(PacketReceivedCallback callback);
    
    using PacketSentCallback = std::function<void(bool success)>;
    virtual void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback = nullptr);

protected:
    virtual void Incoming(std::shared_ptr<Packet> in_packet);
    virtual void Outcoming(std::shared_ptr<Packet> out_packet, PacketSentCallback callback = nullptr);

    TaskQueue send_queue_;
    TaskQueue recv_queue_;

private:
    std::shared_ptr<Transport> lower_;
    PacketReceivedCallback packet_recv_callback_;

    
};

}

#endif