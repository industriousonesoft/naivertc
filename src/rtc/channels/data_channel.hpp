#ifndef _RTC_CHANNELS_DATA_CHANNEL_H_
#define _RTC_CHANNELS_DATA_CHANNEL_H_

#include "base/defines.hpp"
#include "rtc/transports/sctp_message.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/channels/channel.hpp"

#include <memory>
#include <queue>
#include <functional>

namespace naivertc {

// DataTransport
class RTC_CPP_EXPORT DataTransport {
public:
    virtual bool Send(SctpMessageToSend message) = 0;
protected:
    virtual ~DataTransport() = default;
};

// DataChannel
class RTC_CPP_EXPORT DataChannel : public Channel, 
                                   public std::enable_shared_from_this<DataChannel> {
public:
    struct RTC_CPP_EXPORT Init {
        std::string label = "";
        std::string protocol = "";

        // True if ordered delivery is required.
        bool ordered = true;

        // True if the channel has been externally negotiated and we do not send an
        // in-band signalling in the form of an "open" message. If this is true, `id`
        // below must be set; otherwise it should be unset and will be negotiated
        // in-band.
        bool negotiated = false;

        // If set, the maximum number of times this message may be
        // retransmitted by the transport before it is dropped.
        // Setting this value to zero disables retransmission.
        // Valid values are in the range [0-UINT16_MAX].
        // `max_rtx_count` and `max_rtx_ms` may not be set simultaneously.
        std::optional<uint16_t> max_rtx_count = std::nullopt;

        // If set, the maximum number of milliseconds for which the transport
        // may retransmit this message before it is dropped.
        // Setting this value to zero disables retransmission.
        // Valid values are in the range [0-UINT16_MAX].
        // `max_rtx_count` and `max_rtx_ms` may not be set simultaneously.
        std::optional<uint16_t> max_rtx_ms = std::nullopt;

        Init(std::string label);
    };

    using BinaryMessageReceivedCallback = std::function<void(const uint8_t* in_data, size_t in_size)>;
    using TextMessageReceivedCallback = std::function<void(const std::string text)>;
    using BufferedAmountChangedCallback = std::function<void(uint64_t previous_amount)>;
public:
    static std::shared_ptr<DataChannel> RemoteDataChannel(uint16_t stream_id,
                                                          bool negotiated,
                                                          DataTransport* transport);

    static bool IsOpenMessage(const CopyOnWriteBuffer& message);
public:
    DataChannel(const Init& init_config, uint16_t stream_id);
    ~DataChannel();

    uint16_t stream_id() const;
    const std::string label() const;
    const std::string protocol() const;
    bool is_opened() const;

    void HintStreamId(sdp::Role role);

    // TODO: Using peer connection as transport instead of sctp transport.
    void Open(DataTransport* transport);
    void Close() override;
    void RemoteClose();

    void Send(const std::string text);
    void OnIncomingMessage(SctpMessage message);

    void OnOpened(OpenedCallback callback) override;
    void OnClosed(ClosedCallback callback) override;
    void OnMessageReceived(BinaryMessageReceivedCallback callback);
    void OnMessageReceived(TextMessageReceivedCallback callback);
    void OnBufferedAmountChanged(BufferedAmountChangedCallback callback);
    void OnReadyToSend();

private:
    static bool IsCloseMessage(const CopyOnWriteBuffer& message);
    static bool IsAckMessage(const CopyOnWriteBuffer& message);

    static void ParseOpenMessage(const CopyOnWriteBuffer& message, DataChannel::Init& init_config);
    static const CopyOnWriteBuffer CreateOpenMessage(const DataChannel::Init& init_config);
    static const CopyOnWriteBuffer CreateAckMessage();
    static const CopyOnWriteBuffer CreateCloseMessage();

private:
    void ProcessOpenMessage(const SctpMessage& open_message);
    void ProcessPendingIncomingMessages();

    void SendOpenMessage();
    void SendAckMessage();
    void SendCloseMessage();

    void TriggerOpen();
    void TriggerClose();
  
    void Send(SctpMessageToSend message);
    bool TrySend(SctpMessageToSend message);
    void UpdateBufferedAmount(ptrdiff_t delta);
    bool FlushPendingMessages();

    void Reset();
    void CloseStream();

private:
    Init config_;
    uint16_t stream_id_;
    SctpMessageToSend::Reliability user_message_reliability_;
    SctpMessageToSend::Reliability control_message_reliability_;
    
    TaskQueue task_queue_;

    bool is_opened_ = false;
    DataTransport* transport_;

    std::queue<SctpMessageToSend> pending_outgoing_messages_;
    std::queue<SctpMessage> pending_incoming_messages_;

    ptrdiff_t buffered_amount_;

    OpenedCallback opened_callback_ = nullptr;
    ClosedCallback closed_callback_ = nullptr;
    BinaryMessageReceivedCallback binary_message_received_callback_ = nullptr;
    TextMessageReceivedCallback text_message_received_callback_ = nullptr;
    BufferedAmountChangedCallback buffered_amount_changed_callback_ = nullptr;

};

} // namespace naivertc

#endif