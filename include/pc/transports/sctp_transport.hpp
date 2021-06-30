#ifndef _PC_SCTP_TRANSPORT_H_
#define _PC_SCTP_TRANSPORT_H_

#include "base/defines.hpp"
#include "pc/transports/transport.hpp"
#include "common/instance_guard.hpp"

#include <sigslot.h>
#include <usrsctp.h>

namespace naivertc {

class RTC_CPP_EXPORT SctpTransport final : public Transport {
public:
    struct Config {
        uint16_t port;
        // MTU: Maximum Transmission Unit
        std::optional<size_t> mtu;
        // Local max message size at reception
        std::optional<size_t> max_message_size;
    };
public:
    SctpTransport(std::shared_ptr<Transport> lower, Config config);
    ~SctpTransport();

private:
    // usrsctp callbacks
    static void sctp_recv_data_ready_cb(struct socket* socket, void* arg, int flags);
    static int sctp_send_data_ready_cb(void* ptr, const void* data, size_t lent, uint8_t tos, uint8_t set_df);

    void OnSCTPRecvDataIsReady();
    int OnSCTPSendDataIsReady(const void* data, size_t len, uint8_t tos, uint8_t set_df);

private:
    Config config_;

    struct socket* socket_;

    static InstanceGuard<SctpTransport>* s_instance_guard;

};

}

#endif