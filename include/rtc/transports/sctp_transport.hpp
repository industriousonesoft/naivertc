#ifndef _RTC_SCTP_TRANSPORT_H_
#define _RTC_SCTP_TRANSPORT_H_

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
    struct Config {
        // SCTP port
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
    SctpTransport(const Config config, std::shared_ptr<Transport> lower, std::shared_ptr<TaskQueue> task_queue = nullptr);
    ~SctpTransport();

    bool Start() override;
    bool Stop() override;

    void Send(std::shared_ptr<SctpMessage> message, PacketSentCallback callback);
    int Send(std::shared_ptr<SctpMessage> message);
    bool Flush();
    void ShutdownStream(StreamId stream_id);

    using BufferedAmountChangedCallback = std::function<void(StreamId, size_t)>;
    void OnBufferedAmountChanged(BufferedAmountChangedCallback callback);

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

    void Connect();
    void Shutdown();
    void Close();
    void Reset();
    void DoRecv();
    void DoFlush();
    void ResetStream(StreamId stream_id);
    void CloseStream(StreamId stream_id);

    bool FlushPendingMessages();
    int TrySendMessage(std::shared_ptr<SctpMessage> message);
    void UpdateBufferedAmount(StreamId stream_id, ptrdiff_t delta);

    void HandleSctpUpCall();
    bool HandleSctpWrite(const void* data, size_t len, uint8_t tos, uint8_t set_df);

    void ProcessPendingIncomingPackets();
    void ProcessIncomingPacket(std::shared_ptr<Packet> in_packet);
    void ProcessNotification(const union sctp_notification* notification, size_t len);
    void ProcessMessage(BinaryBuffer& message_data, StreamId stream_id, PayloadId payload_id);

    void InitUsrSCTP(const Config& config);
    // usrsctp callbacks
    static void on_sctp_upcall(struct socket* socket, void* arg, int flags);
    static int on_sctp_write(void* ptr, void* in_data, size_t in_size, uint8_t tos, uint8_t set_df);

private:
    void Incoming(std::shared_ptr<Packet> in_packet) override;
    int Outgoing(std::shared_ptr<Packet> out_packet) override;

    void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) override { callback(-1); };
    int Send(std::shared_ptr<Packet> packet) override { return -1; };

    int SendInternal(std::shared_ptr<SctpMessage> message);

private:
    Config config_;
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

    std::queue<std::shared_ptr<SctpMessage>> pending_outgoing_messages_;
    std::map<uint16_t, size_t> stream_buffered_amounts_;

    std::queue<std::shared_ptr<Packet>> pending_incoming_messages_;

    BufferedAmountChangedCallback buffered_amount_changed_callback_ = nullptr;
};

}

#endif