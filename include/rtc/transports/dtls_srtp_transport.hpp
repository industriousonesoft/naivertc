#ifndef _RTC_DTLS_SRTP_TRANSPORT_H_
#define _RTC_DTLS_SRTP_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/dtls_transport.hpp"
#include "rtc/rtp_rtcp/rtp_packet.hpp"

#include <srtp.h>

#include <functional>

namespace naivertc {

class RTC_CPP_EXPORT DtlsSrtpTransport final : public DtlsTransport {
public:
    static void Init();
    static void Cleanup();
public:
    DtlsSrtpTransport(const DtlsTransport::Configuration config, std::shared_ptr<IceTransport> lower, std::shared_ptr<TaskQueue> task_queue = nullptr);
    ~DtlsSrtpTransport();

    void SendRtpPacket(std::shared_ptr<RtpPacket> packet, PacketSentCallback callback);
    int SendRtpPacket(std::shared_ptr<RtpPacket> packet);

    using RtpPacketRecvCallback = std::function<void(std::shared_ptr<RtpPacket>)>;
    void OnReceivedRtpPacket(RtpPacketRecvCallback callback);

private:
    void CreateSrtp();
    void DestroySrtp();
    void InitSrtp();

    void DtlsHandshakeDone() override;
    void Incoming(std::shared_ptr<Packet> in_packet) override;

    bool EncryptPacket(std::shared_ptr<Packet> packet);

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