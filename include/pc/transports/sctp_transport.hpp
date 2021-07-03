#ifndef _PC_SCTP_TRANSPORT_H_
#define _PC_SCTP_TRANSPORT_H_

#include "base/defines.hpp"
#include "pc/transports/transport.hpp"
#include "pc/transports/sctp_message.hpp"
#include "common/instance_guard.hpp"

#include <sigslot.h>
#include <usrsctp.h>
#include <functional>
#include <optional>
#include <vector>
#include <mutex>
#include <queue>
#include <map>

namespace naivertc {

class RTC_CPP_EXPORT SctpTransport final : public Transport {
public:
    struct Config {
        // Data received in the same order it was sent. 
        bool ordered;
        // SCTP port
        uint16_t port;
        // MTU: Maximum Transmission Unit
        std::optional<size_t> mtu;
        // Local max message size at reception
        std::optional<size_t> max_message_size;
    };
public:
    SctpTransport(std::shared_ptr<Transport> lower, const Config& config);
    ~SctpTransport();

    void Start(Transport::StartedCallback callback = nullptr) override;
    void Stop(Transport::StopedCallback callback = nullptr) override;
    void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback = nullptr) override;
    bool Flush();

    sigslot::signal2<uint16_t, size_t> SignalBufferedAmountChanged;

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
    void DoRecv();
    void DoFlush();
    void ResetStream(uint16_t stream_id);
    void CloseStream(uint16_t stream_id);

    bool TrySendQueue();
    bool TrySendMessage(std::shared_ptr<SctpMessage> message);
    void UpdateBufferedAmount(uint16_t stream_id, ptrdiff_t delta);

    void UpdateTransportState(State state);

    void OnSCTPRecvDataIsReady();
    int OnSCTPSendDataIsReady(const void* data, size_t len, uint8_t tos, uint8_t set_df);

    void ProcessNotification(const union sctp_notification* notification, size_t len);
    void ProcessMessage(std::vector<std::byte>&& data, uint16_t stream_id, PayloadId payload_id);

    void InitUsrsctp(const Config& config);
    // usrsctp callbacks
    static void sctp_recv_data_ready_cb(struct socket* socket, void* arg, int flags);
    static int sctp_send_data_ready_cb(void* ptr, const void* data, size_t lent, uint8_t tos, uint8_t set_df);

protected:
    void Incoming(std::shared_ptr<Packet> in_packet) override;
    void Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback = nullptr) override;

private:
    Config config_;
    struct socket* socket_;
    
    static InstanceGuard<SctpTransport>* s_instance_guard;

    static const size_t buffer_size_ = 65536;
	std::byte buffer_[buffer_size_];
    std::vector<std::byte> notification_data_fragments_;
    std::vector<std::byte> message_data_fragments_;

    std::vector<std::byte> string_data_fragments_;
    std::vector<std::byte> binary_data_fragments_;

    size_t bytes_sent_ = 0;
    size_t bytes_recv_ = 0;

    std::mutex waiting_for_sending_mutex_;
    std::condition_variable waiting_for_sending_condition_;
    std::atomic<bool> has_sent_once_ = false;

    std::queue<std::shared_ptr<SctpMessage>> message_queue_;
    std::map<uint16_t, size_t> buffered_amount_;
};

}

#endif