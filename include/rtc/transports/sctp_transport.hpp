#ifndef _RTC_TRANSPORTS_SCTP_TRANSPORT_H_
#define _RTC_TRANSPORTS_SCTP_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/transports/sctp_message.hpp"
#include "rtc/transports/sctp_transport_usr_sctp_settings.hpp"

#include <usrsctp.h>

#include <functional>
#include <optional>
#include <vector>
#include <mutex>
#include <queue>
#include <map>
#include <chrono>

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
    SctpTransport(Configuration config, std::weak_ptr<Transport> lower, std::shared_ptr<TaskQueue> task_queue = nullptr);
    ~SctpTransport();

    bool Start() override;
    bool Stop() override;

    int Send(SctpMessage message);
    bool Flush();
    void CloseStream(uint16_t stream_id);

    using BufferedAmountChangedCallback = std::function<void(uint16_t, size_t)>;
    void OnBufferedAmountChanged(BufferedAmountChangedCallback callback);
    using SctpMessageReceivedCallback = std::function<void(SctpMessage in_packet)>;
    void OnSctpMessageReceived(SctpMessageReceivedCallback callback);
    using ReadyToSendDataCallback = std::function<void(void)>;
    void OnReadyToSendData(ReadyToSendDataCallback callback);

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

    void HandleSctpUpCall();
    bool HandleSctpWrite(const void* data, size_t len, uint8_t tos, uint8_t set_df);
    void OnSctpSendThresholdReached();

    // usrsctp callbacks
    static void on_sctp_upcall(struct socket* socket, void* arg, int flags);
    static int on_sctp_write(void* ptr, void* in_data, size_t in_size, uint8_t tos, uint8_t set_df);
    static int on_sctp_send_threshold_reached(struct socket* socket, uint32_t sb_free, void* ulp_info);

private:
    void Incoming(CopyOnWriteBuffer in_packet) override;
    int Outgoing(CopyOnWriteBuffer out_packet, const PacketOptions& options) override;

    int Send(CopyOnWriteBuffer packet, const PacketOptions& options) override { return -1; };
    int SendInternal(SctpMessage message);

    bool FlushPendingMessages();
    int TrySendMessage(SctpMessage message);
    void UpdateBufferedAmount(uint16_t stream_id, ptrdiff_t delta);
    void ReadyToSend();

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

    std::queue<SctpMessage> pending_outgoing_packets_;
    std::map<uint16_t, size_t> stream_buffered_amounts_;

    std::queue<CopyOnWriteBuffer> pending_incoming_packets_;

    BufferedAmountChangedCallback buffered_amount_changed_callback_ = nullptr;
    SctpMessageReceivedCallback sctp_message_received_callback_ = nullptr;
    ReadyToSendDataCallback ready_to_send_data_callback_ = nullptr;
};

}

#endif