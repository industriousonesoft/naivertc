#ifndef _RTC_TRANSPORTS_SCTP_TRANSPORT_H_
#define _RTC_TRANSPORTS_SCTP_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/transports/sctp_message.hpp"
#include "rtc/transports/sctp_transport_usr_sctp_settings.hpp"
#include "rtc/base/synchronization/event.hpp"

#include <usrsctp.h>

#include <functional>
#include <optional>
#include <queue>

namespace naivertc {

class RTC_CPP_EXPORT SctpTransport final : public Transport {
public:
    struct Configuration {
        // SCTP port, local and remote use the same port
        uint16_t port;
        // MTU: Maximum Transmission Unit
        std::optional<size_t> mtu;
        // Local max message size at reception
        std::optional<size_t> max_message_size;
    };
public:
    static void Init();
    static void CustomizeSctp(const SctpCustomizedSettings& settings);
    static void Cleanup();
public:
    SctpTransport(Configuration config, std::weak_ptr<Transport> lower);
    ~SctpTransport() override;

    bool Start() override;
    bool Stop() override;

    bool Send(SctpMessageToSend message);
    void CloseStream(uint16_t stream_id);

    using ReadyToSendCallback = std::function<void(void)>;
    void OnReadyToSend(ReadyToSendCallback callback);
    using SctpMessageReceivedCallback = std::function<void(SctpMessage message)>;
    void OnSctpMessageReceived(SctpMessageReceivedCallback callback);

private:
    // Order seems wrong but these are the actual values
	// See https://tools.ietf.org/html/draft-ietf-rtcweb-data-channel-13#section-8
    enum class PayloadId : uint32_t {
        PPID_CONTROL = 50,
		PPID_STRING = 51,
		PPID_BINARY_PARTIAL = 52,
		PPID_BINARY = 53,
		PPID_STRING_PARTIAL = 54,
		PPID_STRING_EMPTY = 56,
		PPID_BINARY_EMPTY = 57
    };

    void OpenSctpSocket();
    void ConfigSctpSocket();

    void Connect();
    void Shutdown();
    void Close();
    void Reset();
    void DoRecv();
    void DoFlush();
    void ResetStream(uint16_t stream_id);

    void OnSctpUpCall();
    bool OnSctpWrite(CopyOnWriteBuffer data, uint8_t tos, uint8_t set_df);
    void OnSctpSendThresholdReached();

    // usrsctp callbacks
    static void on_sctp_upcall(struct socket* socket, 
                               void* arg, 
                               int flags);
    static int on_sctp_write(void* ptr, 
                             void* in_data, 
                             size_t in_size, 
                             uint8_t tos, 
                             uint8_t set_df);

private:
    void Incoming(CopyOnWriteBuffer in_packet) override;
    int Outgoing(CopyOnWriteBuffer out_packet, PacketOptions options) override;
    // Disable inherited Send method
    int Send(CopyOnWriteBuffer packet, PacketOptions options) override { return -1; };

    bool SendInternal(SctpMessageToSend message);

    bool FlushPendingMessage();
    bool TrySend(SctpMessageToSend& message);
    void ReadyToSend();

    void HandleSctpUpCall();
    bool HandleSctpWrite(CopyOnWriteBuffer data);

    void ProcessPendingIncomingPackets();
    void ProcessIncomingPacket(CopyOnWriteBuffer in_packet);
    void ProcessNotification(const union sctp_notification* notification, size_t len);
    void ProcessMessage(const BinaryBuffer& message_data, uint16_t stream_id, PayloadId payload_id);

    void ForwardReceivedSctpMessage(SctpMessage message);

private:
    const Configuration config_;
    const PacketOptions packet_options_;
    
    struct socket* socket_ = NULL;
  
    static const size_t buffer_size_ = 65536;
	uint8_t buffer_[buffer_size_];
    BinaryBuffer notification_data_fragments_;
    BinaryBuffer message_data_fragments_;

    BinaryBuffer string_data_fragments_;
    BinaryBuffer binary_data_fragments_;

    size_t bytes_sent_ = 0;
    size_t bytes_recv_ = 0;

    bool has_sent_once_ = false;
    bool ready_to_send_ = false;

    std::optional<SctpMessageToSend> partial_outgoing_packet_;
    std::queue<CopyOnWriteBuffer> pending_incoming_packets_;

    SctpMessageReceivedCallback sctp_message_received_callback_ = nullptr;
    ReadyToSendCallback ready_to_send_callback_ = nullptr;

    Event write_event_;
};

}

#endif