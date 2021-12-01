#ifndef _RTC_TRANSPORTS_DTLS_SRTP_TRANSPORT_H_
#define _RTC_TRANSPORTS_DTLS_SRTP_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/dtls_transport.hpp"

#include <srtp.h>

#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT DtlsSrtpTransport final : public DtlsTransport {
public:
    static void Init();
    static void Cleanup();
public:
    DtlsSrtpTransport(Configuration config, IceTransport* lower, TaskQueue* task_queue);
    ~DtlsSrtpTransport() override;

    int SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options);

    using RtpPacketRecvCallback = std::function<void(CopyOnWriteBuffer, bool /* is_rtcp */)>;
    void OnReceivedRtpPacket(RtpPacketRecvCallback callback);

private:
    void CreateSrtp();
    void DestroySrtp();
    void InitSrtp();

    void DtlsHandshakeDone() override;
    void Incoming(CopyOnWriteBuffer in_packet) override;
    int Outgoing(CopyOnWriteBuffer out_packet, PacketOptions options) override;

    bool EncryptPacket(CopyOnWriteBuffer& packet);
private:
    bool srtp_init_done_;

    srtp_t srtp_in_;
    srtp_t srtp_out_;

    unsigned char client_write_key_[SRTP_AES_128_KEY_LEN + SRTP_SALT_LEN];
    unsigned char server_write_key_[SRTP_AES_128_KEY_LEN + SRTP_SALT_LEN];

    RtpPacketRecvCallback rtp_packet_recv_callback_ = nullptr;
};


} // namespace naivertc


#endif