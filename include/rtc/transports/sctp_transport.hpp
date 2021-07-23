#ifndef _RTC_SCTP_TRANSPORT_H_
#define _RTC_SCTP_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/transports/sctp_packet.hpp"
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
    SctpTransport(std::shared_ptr<Transport> lower, const Config config);
    ~SctpTransport();

    bool Start() override;
    bool Stop() override;

    void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) override;
    int Send(std::shared_ptr<Packet> packet) override;
    bool Flush();

    using SignalBufferedAmountChangedCallback = std::function<void(StreamId, size_t)>;
    void OnSignalBufferedAmountChanged(SignalBufferedAmountChangedCallback callback);

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

    bool FlushPendingPackets();
    int TrySendPacket(std::shared_ptr<SctpPacket> packet);
    void UpdateBufferedAmount(StreamId stream_id, ptrdiff_t delta);

    void HandleSctpUpCall();
    int HandleSctpWrite(const void* data, size_t len, uint8_t tos, uint8_t set_df);

    void ProcessPendingIncomingPackets();
    void ProcessIncomingPacket(std::shared_ptr<Packet> in_packet);
    void ProcessNotification(const union sctp_notification* notification, size_t len);
    void ProcessMessage(std::vector<uint8_t>&& data, StreamId stream_id, PayloadId payload_id);

    int SendInternal(std::shared_ptr<Packet> packet);

    void InitUsrSCTP(const Config& config);
    // usrsctp callbacks
    static void on_sctp_upcall(struct socket* socket, void* arg, int flags);
    static int on_sctp_write(void* ptr, void* in_data, size_t in_size, uint8_t tos, uint8_t set_df);

protected:
    void Incoming(std::shared_ptr<Packet> in_packet) override;
    int Outgoing(std::shared_ptr<Packet> out_packet) override;

private:
    Config config_;
    struct socket* socket_;
  
    static const size_t buffer_size_ = 65536;
	uint8_t buffer_[buffer_size_];
    std::vector<uint8_t> notification_data_fragments_;
    std::vector<uint8_t> message_data_fragments_;

    std::vector<uint8_t> string_data_fragments_;
    std::vector<uint8_t> binary_data_fragments_;

    size_t bytes_sent_ = 0;
    size_t bytes_recv_ = 0;

    bool has_sent_once_ = false;

    std::queue<std::shared_ptr<SctpPacket>> send_packet_queue_;
    std::map<uint16_t, size_t> buffered_amount_;

    std::queue<std::shared_ptr<Packet>> pending_incoming_packets_;

    SignalBufferedAmountChangedCallback signal_buffered_amount_changed_callback_ = nullptr;
};

}

#endif