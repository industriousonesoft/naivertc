#ifndef _RTC_TRANSPORTS_DTLS_SRTP_TRANSPORT_H_
#define _RTC_TRANSPORTS_DTLS_SRTP_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/dtls_transport.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet.hpp"

#include <srtp.h>

#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT DtlsSrtpTransport final : public DtlsTransport {
public:
    static void Init();
    static void Cleanup();
public:
    DtlsSrtpTransport(Configuration config, std::weak_ptr<IceTransport> lower, std::shared_ptr<TaskQueue> task_queue = nullptr);
    ~DtlsSrtpTransport();

    int SendRtpPacket(CopyOnWriteBuffer packet, const PacketOptions& options);

    using RtpPacketRecvCallback = std::function<void(CopyOnWriteBuffer, bool /* is_rtcp */)>;
    void OnReceivedRtpPacket(RtpPacketRecvCallback callback);

private:
    void CreateSrtp();
    void DestroySrtp();
    void InitSrtp();

    void DtlsHandshakeDone() override;
    void Incoming(CopyOnWriteBuffer in_packet) override;
    int Outgoing(CopyOnWriteBuffer out_packet, const PacketOptions& options) override;

    bool EncryptPacket(CopyOnWriteBuffer& packet);
private:
    std::atomic<bool> srtp_init_done_ = false;

    srtp_t srtp_in_;
    srtp_t srtp_out_;

    unsigned char client_write_key_[SRTP_AES_128_KEY_LEN + SRTP_SALT_LEN];
    unsigned char server_write_key_[SRTP_AES_128_KEY_LEN + SRTP_SALT_LEN];

    RtpPacketRecvCallback rtp_packet_recv_callback_ = nullptr;
};


} // namespace naivertc


#endif