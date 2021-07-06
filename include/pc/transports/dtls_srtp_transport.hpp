#ifndef _DTLS_SRTP_TRANSPORT_H_
#define _DTLS_SRTP_TRANSPORT_H_

#include "base/defines.hpp"
#include "pc/transports/dtls_transport.hpp"

#include <srtp.h>

namespace naivertc {

class RTC_CPP_EXPORT DtlsSrtpTransport final : public DtlsTransport {
public:
    static void Init();
    static void Cleanup();
public:
    DtlsSrtpTransport(std::shared_ptr<IceTransport> lower, const DtlsTransport::Config& config);
    ~DtlsSrtpTransport();

    void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback = nullptr) override;

private:
    void InitSrtp();
    void DeinitSrtp();

    void HandshakeDone() override;
    void Incoming(std::shared_ptr<Packet> in_packet) override;
    
private:
    std::atomic<bool> srtp_init_done_ = false;

    srtp_t srtp_in_;
    srtp_t srtp_out_;

    unsigned char client_write_key_[SRTP_AES_128_KEY_LEN + SRTP_SALT_LEN];
    unsigned char server_write_key_[SRTP_AES_128_KEY_LEN + SRTP_SALT_LEN];

};


} // namespace naivertc


#endif